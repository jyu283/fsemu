#include "fs.h"
#include "fs_syscall.h"
#include "fsemu.h"

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
static inline void free_dent(struct dentry *dent)
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
		return -1;
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
		return -1;
	}
	if (new_dentry(dir, parent, "..") < 0) {
		printf("Error: failed to create .. dentry.\n");
		return -1;
	}
	return 0;
}

/*
 * Create a file/directory/device.
 * This is *not* the system call.
 */
int do_creat(struct inode *dir, const char *name, uint8_t type)
{
	if (dir->type != T_DIR) {
		printf("Error: invalid inode type.\n");
		return -1;
	}
	if (strlen(name) + 1 > DENTRYNAMELEN) {
		printf("creat: name too long.\n");
		return -1;
	}
	struct inode *inode = alloc_inode(type);
	if (!inode) {
		printf("Error: failed to allocate inode.\n");
		return -1;
	}
	if (new_dentry(dir, inode, name) < 0) {
		printf("Error: failed to create dentry %s.\n", name);
		return -1;
	}
	if (type == T_DIR) {
		if (init_dir_inode(inode, dir) < 0)
			return -1;
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
static void init_fs(size_t size)
{
	init_superblock(size);	// fill in superblock
	init_rootdir();			// create root directory
	init_fd();				// create file descriptors
	pr_debug("File system initialization completed!\n");
}

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
static struct dentry *dir_lookup(const struct inode *dir, const char *name)
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
 * Lookup the provided pathname.
 * Currently no support for relative pathnames.
 * For now pathname is just a file name, assumed to be in root.
 */
static struct dentry *lookup(const char *pathname)
{
	struct dentry *dent;
	
	dent = dir_lookup(sb->rootdir.inode, pathname);
	return dent;
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
	return -1;
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
		return -1;
	if ((dent = lookup(pathname)) == NULL)
		return -1;

	openfiles[fd].f_dentry = dent;
	openfiles[fd].offset = 0;
	return fd;
}

/**
 * Basic version of the POSIX close system call.
 */
int fs_close(int fd)
{
	if (fd < 0 || fd >= MAXOPENFILES)
		return -1;

	openfiles[fd].f_dentry = NULL;
	openfiles[fd].offset = 0;
	return 0;
}

/**
 * Basic version of the POSIX unlink system call.
 * Again, no support yet for relative paths or even path 
 * interpretation. By default, we assume pathname to simply
 * be a filename in the root directory.
 */
int fs_unlink(const char *pathname)
{
	struct inode *parentdir = sb->rootdir.inode;
	struct dentry *dent = lookup(pathname);
	if (!dent)
		return -1;
	if (dent->inode->type == T_DIR)
		return -1;
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
		return -1;
	struct inode *inode = olddent->inode;
	if (inode->type == T_DIR)
		return -1;

	// make sure newpath doesn't already exist
	if (lookup(newpath))
		return -1;

	return new_dentry(sb->rootdir.inode, inode, newpath);
}

/**
 * Basic version of the POSIX mkdir() system call.
 */
int fs_mkdir(const char *pathname)
{
	if (lookup(pathname))
		return -1;

	return do_creat(sb->rootdir.inode, pathname, T_DIR);
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
				return -1;
		}
	}
	return 0;
}

/**
 * Basic version of the POSIX rmdir() system call.
 */
int fs_rmdir(const char *pathname)
{
	struct inode *parent = sb->rootdir.inode;
	struct dentry *dent = lookup(pathname);
	if (dent->inode->type != T_DIR)
		return -1;
	if (!dir_isempty(dent->inode))
		return -1;

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
	if ((fd = open("/dev/zero", O_RDWR)) < 0) {
		perror("open");
		return -1;
	}
	fs = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (!fs) {
		perror("mmap");
		return -1;
	}
	close(fd); 
	init_fs(size);
	return 0;
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
	if ((write(fd, fs, sb->size)) < 0) {
		perror("write");
	}
	close(fd);
}

/**
 * Unmounts the file system.
 */
int fs_unmount(void)
{
	dump_fs();
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