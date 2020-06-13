/**
 * fsemu/src/fs.c
 * 
 * The core of the file system. For now this is where all the 
 * file system-related system calls are defined.
 */

#include "fs.h"
#include "fs_syscall.h"
#include "fserror.h"
#include "util.h"
#include "fsemu.h"
#include "dentry_cache.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

char *fs;
struct superblock *sb;

/*
 * Calculate the positions of each region of the file system.
 *  - Superblock is the very first block in the file system.
 *  - Inode region should be aboud 3% of the entire file system.
 *  - Bitmap maps all the blocks in the file system.
 *  - The remaining are data blocks.
 */
static void init_superblock(size_t size)
{
	sb = (struct superblock *)fs;

	int total_blocks = size / BSIZE;
	int inode_blocks = total_blocks * 0.03;
	sb->size = total_blocks;
	pr_debug("Total blocks: %d.\n", total_blocks);

	sb->inodes = (struct inode *)(fs + BSIZE);
	sb->ninodes = inode_blocks * INOPERBLK; 
	pr_debug("Allocated %d blocks for inodes at %p.\n", 
				inode_blocks, sb->inodes);

	sb->bitmap = fs + BSIZE + inode_blocks * BSIZE;

	// Here, number of data blocks are overestimated for convenience
	int bitmap_blocks = (total_blocks / 8) / BSIZE;
	pr_debug("Allocated %d blocks for the bitmap at %p.\n", 
				bitmap_blocks, sb->bitmap);

	int nblocks = total_blocks - 1 - inode_blocks - bitmap_blocks;
	sb->datastart = inode_blocks + bitmap_blocks;
	sb->nblocks = nblocks;
	pr_debug("Allocated %d data blocks starting block %lu.\n", 
				nblocks, sb->datastart);
}

/**
 * Zero out a data block before allocating it to an inode.
 */
static void wipe_block(uint32_t blocknum)
{
	char *block = BLKADDR(blocknum);
	memset(block, 0, BSIZE);
}

/*
 * Consult the bitmap and allocate a datablock.
 */
static uint32_t alloc_data_block(void)
{
	uint32_t block = 0;
	char *bm = sb->bitmap;
	for (int i = 0; i < sb->size; i++) {
		if (bm[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (((bm[i] >> j) & 1) == 0) {
					bm[i] |= 1 << j;
					block = sb->datastart + (i * 8 + j);
					wipe_block(block);
					goto out;
				}
			}
		}
	}
out:
	return block;
}

/**
 * Free data block number b.
 */
static void free_data_block(uint32_t b)
{
	char *bm = sb->bitmap;
	int byte = b / 8;
	int bit = b % 8;
	bm[byte] &= ~(1 << bit);
}

/*
 * Locates an unused (T_UNUSED) inode from the inode pool
 * and returns a pointer to that inode. Also initializes 
 * the type field.
 */
static struct inode *alloc_inode(uint8_t type)
{
	/* Inode 0 is reserved! */
	struct inode *inode = sb->inodes + 1;
	for (; inode < sb->inodes + sb->ninodes; inode++) {
		if (inode->type == T_UNUSED) {
			inode->type = type;
			inode->size = 0;
			return inode;
		}
	}
	return NULL;
}

/**
 * Frees an inode and all the data blocks it uses.
 * 
 * Do not call this function to remove a file!
 * Use free_dent instead which will call this function.
 */
static int free_inode(struct inode *inode)
{
	if (inode->nlink > 0)
		return -1;

	for (int i = 0; i < NADDR; i++) {
		if (inode->data[i]) {
			free_data_block(inode->data[i]);
			inode->data[i] = 0;
		}
	}
	inode->type = T_UNUSED;
	return 0;
}

/*
 * Find an unused dentry spot from directory inode.
 */
static struct dentry *get_unused_dentry(struct inode *dir)
{
	struct dentry *dents;
	int unused = -1;	// record of first unused block
	for (int i = 0; i < NADDR - 1; i++) {
		if (dir->data[i]) {
			dents = BLKADDR(dir->data[i]);
			for (int j = 0; j < DENTPERBLK; j++) {
				if (dents[j].inode == NULL)
					return &dents[j];
			}
		} else {
			if (unused == -1) {
				unused = i;
			}
		}
	}

	if (unused < 0)
		return NULL;

	// allocate new data block to unused address
	if ((dir->data[unused] = alloc_data_block()) == 0) {
		printf("Error: data allocation failed.\n");
		return NULL;
	}

	dir->size += BSIZE;  // increase directory size by one block
	return (struct dentry *)BLKADDR(dir->data[unused]);
}

/**
 * Initialize a dentry with a name and link it to the inode.
 * As a result of this linkage, the inode's nlink is incremented.
 */
static inline void init_dent(struct dentry *dent, struct inode *inode,
							const char *name)
{
	dent->inode = inode;
	inode->nlink++;
	strcpy(dent->name, name);
}

/**
 * This should be the function called when removing a file!
 * 
 * Free a dentry and unlink it from the inode.
 * As a result of this unlinking, the inode's nlink is decremented.
 */
static void free_dent(struct dentry *dent)
{
	struct inode *inode = dent->inode;

	dent->inode = NULL;
	dent->name[0] = '\0';

	/* This check should be redundant. */
	if (inode) {
		inode->nlink--;	 // Can be done in if clause. I know. Keep quiet.
		// If inode nlink becomes 0, deallocate that inode.
		if (inode->nlink == 0) {
			free_inode(inode);
		}
	}
}

/**
 * Find an unused dentry in dir and initialize it with
 * a name and an inode.
 */
static int new_dentry(struct inode *dir, struct inode *inode, const char *name)
{
	struct dentry *dent = get_unused_dentry(dir);	
	if (!dent)
		return -EALLOC;
	init_dent(dent, inode, name);
	return 0;
}

/*
 * Create the . and .. dentries for a directory inode.
 */
static int init_dir_inode(struct inode *dir, struct inode *parent)
{
	if (new_dentry(dir, dir, ".") < 0) {
		printf("Error: failed to create . dentry.\n");
		return -EALLOC;
	}
	if (new_dentry(dir, parent, "..") < 0) {
		printf("Error: failed to create .. dentry.\n");
		return -EALLOC;
	}
	return 0;
}

/*
 * Create a file/directory/device.
 * This is *not* the system call.
 */
int do_creat(struct inode *dir, const char *name, uint8_t type)
{
	int ret;

	if (dir->type != T_DIR) {
		printf("Error: invalid inode type.\n");
		return -EINVTYPE;
	}
	if (strlen(name) + 1 > DENTRYNAMELEN) {
		printf("creat: name too long.\n");
		return -EINVNAME;
	}
	struct inode *inode = alloc_inode(type);
	if (!inode) {
		printf("Error: failed to allocate inode.\n");
		return -EALLOC;
	}
	if (new_dentry(dir, inode, name) < 0) {
		printf("Error: failed to create dentry %s.\n", name);
		return -EALLOC;
	}
	if (type == T_DIR) {
		if ((ret = init_dir_inode(inode, dir)) < 0)
			return ret;
	}
	return 0;
}

/*
 * Initializes the root directory by allocating an inode.
 */
static void init_rootdir(void)
{
	struct inode *rootino = alloc_inode(T_DIR);     
	init_dent(&sb->rootdir, rootino, "/");
	init_dir_inode(rootino, rootino);	// root inode parent is self
}

static void init_fd(void)
{
	for (int i = 0; i < MAXOPENFILES; i++) {
		openfiles[i].f_dentry = NULL;
		openfiles[i].offset = 0;
	}
}

/*
 * Master function to initialize the file system.
 */
static int init_fs(size_t size)
{
	init_superblock(size);	// fill in superblock
	init_rootdir();			// create root directory
	init_fd();				// create file descriptors
	if (dentry_cache_init_all() < 0) {
		printf("Error: failed to initialize dentry cache.\n");
		return -1;
	}
	pr_debug("File system initialization completed!\n");
	return 0;
}

/**
 * Locate a dentry in a block of dentries.
 */
static struct dentry *find_dent_in_block(uint32_t blocknum, const char *name)
{
	struct dentry *dents = (struct dentry *)BLKADDR(blocknum);
	for (int i = 0; i < DENTPERBLK; i++) {
		if (dents[i].inode && strcmp(dents[i].name, name) == 0) {
			return &dents[i];
		}
	}
	return NULL;
}

/**
 * Lookup a dentry in a given directory (inode).
 * Helper function for lookup() but for now it's basically all
 * lookup() does.
 */
static struct dentry *lookup_dent(const struct inode *dir, const char *name)
{
	struct dentry *dent = NULL;
	for (int i = 0; i < NADDR - 1; i++) {
		if (dir->data[i]) {
			dent = find_dent_in_block(dir->data[i], name);
			if (dent)
				return dent;
		}
	}
	return NULL;
}

/**
 * Get the next part (component) of the pathname separated by '/'.
 * The component is filled into the char array provided by lookup().
 */
static int get_path_component(const char **pathname, char *component)
{
	while (**pathname == '/')
		*pathname += 1;

	if (**pathname == '\0')
		return 0;

	int i = 0;
	while (**pathname != '/' && **pathname != '\0') {
		component[i++] = **pathname;
		*pathname += 1;
	}
	component[i] = '\0';
	return i;
}

/**
 * Check if a pathname is empty except for separators.
 */
static int path_is_empty(const char *pathname)
{
	const char *c = pathname;
	while (*c == '/')
		c++;

	// if path is empty, we should be at the end
	return (*c == '\0');
}

/**
 * Get the name of the file from the full path name.
 */
static void get_filename(const char *pathname, char *name)
{
	int start, end, len;
	
	end = strlen(pathname) - 1;
	while (pathname[end] == '/')
		end--;
	start = end;
	while (pathname[start - 1] != '/')
		start--;
	len = end - start + 1;
	strncpy(name, &pathname[start], len);
	name[len] = '\0';
}


/*
 * Different types/usages of lookup:
 * 
 * 1. In determining if a file exists:
 *     Need to go all the way to the last component of the pathname,
 * and return the dentry of that last component, or NULL if any component
 * along the way does not exist. 
 * 
 * 2. In creating a new file:
 *     Need to check if the file already exists (1) and then also
 * obtain a pointer to the parent directory in order to create the 
 * new file. It is okay if the last component does not exist, but not okay
 * if a component along the way does not exist.
 * 
 * Also, every component except the last should be a directory. This should
 * also be checked during lookup.  
 * 
 * Conclusion: need to know when the lookup has reached the last component
 * of the pathname, and lookup needs to somehow return the second-to-last
 * component to the caller, preferrably in the form of an inode pointer.
 * 
 */

/**
 * Lookup the provided pathname. (No relative pathnames.) 
 * Even if pathname does not start with '/', the file system
 * will still use the root directory as a starting point.
 */
static struct dentry *do_lookup(const char *pathname, struct inode **pi)
{
	struct dentry *dent, *curr;
	struct inode *prev;
	char component[DENTRYNAMELEN + 1] = { '\0' };
	
	prev = sb->rootdir.inode;

	// FIXME: lookup would fail if called with "/"
	while (get_path_component(&pathname, component)) {
		if (!(dent = lookup_dent(prev, component))) {
			// If lookup failed on the last component, then fill
			// in the pi field. Otherwise, set pi field to NULL
			// to indicate that lookup failed along the way.
			if (!path_is_empty(pathname))
				prev = NULL;
			break;
		}
		if (!path_is_empty(pathname))
			prev = dent->inode;	
	}
	// pr_debug("\n\n");
	if (pi)
		*pi = prev;
	return dent;
}

/**
 * Regular lookup. Searches for the file specified by pathname
 * and return the resulting dentry or NULL if file isn't found.
 */
struct dentry *lookup(const char *pathname)
{
	return do_lookup(pathname, NULL);
}

/**
 * Use this lookup in system calls that involve modifying things.
 * (e.g. mkdir, rmdir, creat, etc.) 
 * 
 * @param pathname The pathname to lookup
 * @param pi dir_lookup() will store the inode of the parent dir. 
 * This will be NULL if anything but the last component in the path 
 * does not exist.
 * @return The dentry to the file in question.
 */
struct dentry *dir_lookup(const char *pathname, struct inode **pi)
{
	return do_lookup(pathname, pi);
}

/**
 * Find an unused file struct and return its index 
 * as the file descriptor.
 */
static int get_open_fd(void)
{
	for (int i = 0; i < MAXOPENFILES; i++) {
		if (!(openfiles[i].f_dentry)) {
			return i;
		}
	}
	return -ENOFD;
}

/**
 * Basic version of the POSIX creat system call.
 * Currently no support for flags/modes.
 * 
 * For onw creates a T_REG file by default.
 */
int fs_creat(const char *pathname)
{
	int fd;
	struct dentry *dent;
	struct inode *dir;

	if ((dent = dir_lookup(pathname, &dir)))
		return -EEXISTS;
	if (!dir)
		return -ENOFOUND;

	char filename[DENTRYNAMELEN + 1];
	get_filename(pathname, filename);

	return do_creat(dir, filename, T_REG);
}

/**
 * Basic version of the POSIX open system call.
 * Currently no support for flags/modes as there are no
 * processes yet.
 */
int fs_open(const char *pathname)
{
	int fd;
	struct dentry *dent;

	if ((fd = get_open_fd()) < 0)
		return fd;	// ENOFD
	if ((dent = lookup(pathname)) == NULL)
		return -ENOFOUND;
	if (dent->inode->type == T_DIR)
		return -EINVTYPE;

	openfiles[fd].f_dentry = dent;
	openfiles[fd].offset = 0;
	return fd;
}

/**
 * Check if a give file descriptor is actually in use.
 */
static int fd_inuse(int fd)
{
	if (fd < 0 || fd >= MAXOPENFILES)
		return -1;
	if (!openfiles[fd].f_dentry)
		return -1;
	return 1;
}

/**
 * Basic version of the POSIX close system call.
 */
int fs_close(int fd)
{
	if (!fd_inuse(fd))
		return -EINVFD;

	openfiles[fd].f_dentry = NULL;
	openfiles[fd].offset = 0;
	return 0;
}

/**
 * Basic version of the POSIX lseek system call.
 * Future versions will implement whence. (see LSEEK(2))
 */
unsigned int fs_lseek(int fd, unsigned int off)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (off < 0 || off > openfiles[fd].f_dentry->inode->size)
		return -EINVAL;

	openfiles[fd].offset = off;
	return off;
}

/**
 * Get the actual address for the offset in the given file.
 */
static char *get_off_addr(struct inode *file, unsigned int off)
{
	char *addr = NULL;
	int b = off / BSIZE;
	if (file->data[b]) {
		addr = BLKADDR(file->data[b]) + off % BSIZE;
	}
	return addr;
}

/**
 * Read from a file.
 * 
 * @param file	The file's inode
 * @param off	The starting offset
 * @param buf	Buffer wherein bytes read are stored
 * @param n		Number of bytes requested
 * @return		The number of bytes read
 * 
 */
unsigned int do_read(struct inode *file, unsigned int off, 
					 void *buf, unsigned int n)
{
	int nread = 0;
	int size;	// size of each copy
	char *start;

	while (n > 0 && off < file->size) {
		if (!(start = get_off_addr(file, off)))
			goto out;

		// determine what the copy size should be
		size = BSIZE - off % BSIZE;  // rest of the block.
		if (file->size < off + size)
			size = file->size - off;  // rest of the file.
		if (nread + size > n)
			size = n - nread;  // rest of requested bytes.
		memcpy(buf + nread, start, size);
		n -= size;
		nread += size;
		off += size;
	}

out:
	return nread;
}

/**
 * Basic version of the POSIX read system call.
 * 
 * @param fd	File descriptor to read from
 * @param buf	Buffer wherein bytes read are stored
 * @param count	Number of bytes requested
 * @return		Number of bytes actually read
 */
unsigned int fs_read(int fd, void *buf, unsigned int count)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (!buf)
		return -1;	

	struct inode *file = openfiles[fd].f_dentry->inode;
	if (file->type != T_REG)
		return -EINVTYPE;

	int ret;
	unsigned int off = openfiles[fd].offset;
	ret = do_read(file, off, buf, count);
	if (ret < 0)
		goto bad_read;
	openfiles[fd].offset += ret;

bad_read:
	return ret;
}

/**
 * Basic version of the POSIX unlink system call.
 * Again, no support yet for relative paths or even path 
 * interpretation. By default, we assume pathname to simply
 * be a filename in the root directory.
 */
int fs_unlink(const char *pathname)
{
	struct inode *dir;
	struct dentry *dent;
	if (!(dent = dir_lookup(pathname, &dir)) || !dir)
		return -ENOFOUND;
	if (dent->inode->type == T_DIR)
		return -EINVTYPE;
	free_dent(dent);
	return 0;
}

/**
 * Basic version of the POSIX link system call.
 * No support for sophisticated path lookup. Yada yada yada.
 */
int fs_link(const char *oldpath, const char *newpath)
{
	struct dentry *olddent = lookup(oldpath);
	if (!olddent)
		return -ENOFOUND;
	struct inode *inode = olddent->inode;
	if (inode->type == T_DIR)
		return -EINVTYPE;

	// make sure newpath doesn't already exist
	struct inode *dir;
	if (dir_lookup(newpath, &dir))
		return -EEXISTS;
	if (!dir)
		return -ENOFOUND;

	char filename[DENTRYNAMELEN + 1];
	get_filename(newpath, filename);
	return new_dentry(dir, inode, filename);
}

/**
 * Basic version of the POSIX mkdir() system call.
 */
int fs_mkdir(const char *pathname)
{
	struct inode *dir;

	if (dir_lookup(pathname, &dir))
		return -EEXISTS;
	if (!dir)
		return -ENOFOUND;

	char filename[DENTRYNAMELEN + 1];
	get_filename(pathname, filename);

	return do_creat(dir, filename, T_DIR);
}

/**
 * Check if a block of dentries contains no valid dentries
 * (apart from the . and .. entries).
 */
static int dir_block_isempty(uint32_t b)
{
	struct dentry *dents = (struct dentry *)BLKADDR(b);
	for (int i = 0; i < DENTPERBLK; i++) {
		if (dents[i].inode) {
			if (strcmp(dents[i].name, ".") == 0)
				continue;
			if (strcmp(dents[i].name, "..") == 0)
				continue;
			// valid dentry is neither . nor ..
			return -1;
		}
	}
	return 0;
}

/**
 * Check if a directory is empty.
 */
static int dir_isempty(struct inode *dir)
{
	for (int i = 0; i < NADDR; i++) {
		if (dir->data[i]) {
			if (!dir_block_isempty(dir->data[i]))
				return -ENOTEMPTY;
		}
	}
	return 0;
}

/**
 * Basic version of the POSIX rmdir() system call.
 */
int fs_rmdir(const char *pathname)
{
	struct inode *parent;
	struct dentry *dent = dir_lookup(pathname, &parent);
	if (!dent || !parent)
		return -ENOFOUND;
	if (dent->inode->type != T_DIR)
		return -EINVTYPE;
	if (!dir_isempty(dent->inode))
		return -ENOTEMPTY;

	free_dent(dent);
	parent->nlink--;
	return 0;
}

/*
 * Allocates space for file system in memory.
 */
int fs_mount(unsigned long size)
{
	pr_debug("Allocating %lu bytes for file system...\n", size);

	int fd;
	if (size > MAXFSSIZE) {
		printf("Error: file system size cannot be greater than 1GB.\n");
		return -1;
	}

	fd = open("fs.img", O_RDWR | O_CREAT, 0666);
	if (!fd) {
		perror("open");
		return -1;
	}
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0) {
		perror("stat");
		return -1;
	}
	if (statbuf.st_size != size) {
		ftruncate(fd, 0);
		lseek(fd, size, SEEK_SET);
		write(fd, "\0", 1);
	}
	fs = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!fs) {
		perror("mmap");
		return -1;
	}
	close(fd); 
	return init_fs(size);
}

/*
 * Create a dump of the entire file system
 */
static void dump_fs(void)
{
	int fd;
	if ((fd = open("fs.img", O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
		perror("open");
		return;
	}
	printf("Dumping file system to fs.img...\n");
	if ((write(fd, fs, sb->size * BSIZE)) < 0) {
		perror("write");
	}
	close(fd);
}

/**
 * Unmounts the file system.
 */
int fs_unmount(void)
{
	dentry_cache_free_all();
	munmap(fs, sb->size * BSIZE);
	return 0;
}

/* 
 * Debug function:
 * Create a file/directory/device at root directory.
 */
int db_creat_at_root(const char *name, uint8_t type)
{
	return do_creat(sb->rootdir.inode, name, type);
}

/*
 * Debug function:
 * Create a directory inode at root directory.
 */
int db_mkdir_at_root(const char *name)
{
	return do_creat(sb->rootdir.inode, name, T_DIR);
}