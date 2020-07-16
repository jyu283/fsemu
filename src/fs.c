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

#ifdef PCACHE_ENABLED
#include "pcache.h"
#endif  // DCACHE_ENABLED
#ifdef DCACHE_ENABLED
#include "dentry_cache.h"
#endif  // PCACHE_ENABLED

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

char *fs = NULL;
struct superblock *sb;
struct inode *inodes;
char *bitmap;

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

	sb->inodestart = 1;
	sb->ninodes = inode_blocks * INOPERBLK; 

	sb->bitmapstart = sb->inodestart + inode_blocks;

	// Here, number of data blocks are overestimated for convenience
	int bitmap_blocks = (total_blocks / 8) / BSIZE;

	int nblocks = total_blocks - 1 - inode_blocks - bitmap_blocks;
	sb->datastart = inode_blocks + bitmap_blocks;
	sb->nblocks = nblocks;

	inodes = BLKADDR(sb->inodestart);
	bitmap = BLKADDR(sb->bitmapstart);
}

/**
 * Read from an existing superblock to populate the global variables.
 */
static inline void read_sb()
{
	sb = (struct superblock *)fs;
	inodes = BLKADDR(sb->inodestart);
	bitmap = BLKADDR(sb->bitmapstart);
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
 * 
 * Returns 0 for failure, positive int for allocated block number.
 */
static uint32_t alloc_data_block(void)
{
	uint32_t block = 0;
	for (int i = 0; i < sb->size; i++) {
		if (bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (((bitmap[i] >> j) & 1) == 0) {
					bitmap[i] |= 1 << j;
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
	char *bm = BLKADDR(sb->bitmapstart);
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
	struct inode *inode;
	for (int i = 1; i < sb->ninodes; i++) {
		inode = &inodes[i];
		if (inode->type == T_UNUSED) {
			memset((void *)inode, 0x0, sizeof(struct inode));
			inode->type = type;
			// Enable inline by default for directories.
			if (type == T_DIR) 
				inode->flags |= I_INLINE;
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

	for (int i = 0; i < NBLOCKS; i++) {
		if (inode->data.blocks[i]) {
			free_data_block(inode->data.blocks[i]);
			inode->data.blocks[i] = 0;
		}
	}
	inode->type = T_UNUSED;
	return 0;
}

/**
 * Find an unused dentry spot from a block.
 */
static struct dentry *get_unused_dentry_from_block(uint32_t block_num)
{
	struct dentry_block *block = BLKADDR(block_num);
	for (int j = 0; j < DENTPERBLK; j++) {
		if (block->dents[j].inum == 0)
			return &block->dents[j];
	}
	return NULL;
}

/*
 * Find an unused dentry spot from directory inode.
 */
static struct dentry *get_unused_dentry(struct inode *dir)
{
	struct dentry_block *block;
	struct dentry *dent;
	int unused = -1;
	for (int i = 0; i < NBLOCKS - 1; i++) {
		if (dir->data.blocks[i]) {
			dent = get_unused_dentry_from_block(dir->data.blocks[i]);
			if (dent)
				return dent;
		} else {
			if (unused == -1)
				unused = i;
		}
	}

	if (unused < 0)
		return NULL;

	// allocate new data block to unused address
	if ((dir->data.blocks[unused] = alloc_data_block()) == 0) {
		printf("Error: data allocation failed.\n");
		return NULL;
	}

	dir->size += BSIZE;  // increase directory size by one block
	return (struct dentry *)BLKADDR(dir->data.blocks[unused]);
}

/**
 * Initialize a dentry with a name and link it to the inode.
 * As a result of this linkage, the inode's nlink is incremented.
 */
static inline void init_regular_dent(struct dentry *dent, struct inode *inode,
							const char *name)
{
	dent->inum = inum(inode);
	inode->nlink++;
	strcpy(dentry_get_name(dent), name);
	dent->name_hash = dentry_hash(name);
}

/**
 * This should be the function called when removing a file!
 * 
 * Free a dentry and unlink it from the inode.
 * As a result of this unlinking, the inode's nlink is decremented.
 */
static void free_dent(struct dentry *dent)
{
	struct inode *inode = dentry_get_inode(dent);

	dent->inum = 0;
	dent->name_hash = 0;
	dentry_get_name(dent)[0] = '\0';

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
 * Create a new inline directory entry.
 */
static struct dentry *new_inline_dentry(struct inode *dir,
										struct inode *inode, const char *name)
{
	pr_debug("Attempting to create new inline dentry %s...\n", name);
	struct dentry_inline *dent;
	for_each_inline_dent(dent, dir) {
		if (dent->inum != 0)
			continue;
		uint8_t reclen = get_inline_dent_reclen(name);	
		uint8_t namelen = strlen(name) + 1;

		// This dentry is not large enough
		if (dent->reclen != 0 && dent->reclen < reclen)
			continue;

		// This is a never-before-used dentry.
		if (dent->reclen == 0) {
			if (inline_dent_can_fit(dir, dent, reclen))
				dent->reclen = reclen;
			else
				break;
		}

		dent->namelen = namelen;
		dent->inum = inum(inode);
		strcpy(dent->name, name);

		/* * * * * * * * * * * * * * * * * * * * * * * * * * * * 
			* WARNING: 
			* Do not use this return value. This needs to be fixed.
			* * * * * * * * * * * * * * * * * * * * * * * * * * * */
		return (struct dentry *)dent;	// FIXME
	}
	return NULL;
}

/**
 * Converts an inline directory to a regular directory.
 * 
 * First, allocate a new block for this directory, then fill in the
 * "." and ".." dentries. Finally, convert all inline entries to
 * regular entries and place them also in the block, which is then
 * attached to the inode.
 */
static int dir_convert_inline_to_reg(struct inode *dir)
{
	if (!inode_is_inline_dir(dir))
		return -1;

	pr_debug(":: Converting inline directory to regular directory.\n");

	struct dentry *reg_dent;
	uint32_t block = alloc_data_block();
	if (!block) 
		return -1;

	// dot entry
	reg_dent = get_unused_dentry_from_block(block);
	init_regular_dent(reg_dent, dir, ".");
	// dot dot entry
	reg_dent = get_unused_dentry_from_block(block);
	init_regular_dent(reg_dent, &inodes[dir->data.inline_dir.p_inum], "..");

	// Conver the inline directory entries and fill in the block.
	struct dentry_inline *inline_dent;
	for_each_inline_dent(inline_dent, dir) {
		if (!inline_dent->reclen)
			break;
		reg_dent = get_unused_dentry_from_block(block);	
		init_regular_dent(reg_dent, 
						&inodes[inline_dent->inum], inline_dent->name);
	}

	// Unset the inline bit in flag
	dir->flags &= ~I_INLINE;

	// Wipe out the inode.data area, and set the first block
	// Thus dawns a new age for this directory inode.
	memset(&dir->data, 0x0, sizeof(dir->data));
	dir->data.blocks[0] = block;
	return 0;
}

/**
 * Find an unused dentry in dir and initialize it with
 * a name and an inode.
 * 
 * FIXME: WARNING: DO NOT USE RETURN VALUE. See warning in fs.h
 */
static struct dentry *new_dentry(struct inode *dir, 
								struct inode *inode, const char *name)
{
	struct dentry *dent;
	if (inode_is_inline_dir(dir)) {
		if ((dent = new_inline_dentry(dir, inode, name)))
			return dent;  // DO NOT USE THIS RETURN VALUE
		// If failed to allocate inline, convert directory inode
		// to regular mode.
		dir_convert_inline_to_reg(dir);
	}

	dent = get_unused_dentry(dir);	
	if (!dent)
		return NULL;
	init_regular_dent(dent, inode, name);
	return dent;
}

/*
 * Create the . and .. dentries for a directory inode.
 * 
 * OR: If inline directories are enabled, simply use the first 
 * 4 bytes of inode.data to store the parent dir's inum.
 */
static int init_dir_inode(struct inode *dir, struct inode *parent)
{
	// Inline directory
	if (inode_is_inline_dir(dir)) {
		dir->data.inline_dir.p_inum = inum(parent);
		dir->data.inline_dir.dent.inum = 0;
		return 0;
	}

	// Regular directory
	if (!new_dentry(dir, dir, ".")) {
		printf("Error: failed to create . dentry.\n");
		return -EALLOC;
	}
	if (!new_dentry(dir, parent, "..")) {
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
	if (!new_dentry(dir, inode, name)) {
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
	pr_debug("Root inum: %d\n", inum(rootino));
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

	pr_debug("File system initialization completed!\n");
	return 0;
}

/**
 * Initialize in-memory caches.
 */
static int init_caches(void)
{
#ifdef DCACHE_ENABLED
	if (dentry_cache_init_all() < 0) {
		printf("Error: failed to initialize dentry cache.\n");
		return -1;
	}
#endif  // DCACHE_ENABLED

#ifdef PCACHE_ENABLED
	if (pcache_init() < 0) {
		printf("Error: failed to initialize path cache.\n");
		return -1;
	}
#endif  // PCACHE_ENABLED

	return 0;
}

/**
 * Locate a dentry in a block of dentries.
 * 
 * Cache-light split dentry:
 * First check dent->name_hash to quickly dismiss non-matching dentries.
 * If name_hash is a match, then use strcmp on the full name to confirm.
 */
static struct dentry *find_dent_in_block(uint32_t blocknum, const char *name)
{
	struct dentry_block *block = (struct dentry_block*)BLKADDR(blocknum);
	unsigned long name_hash = dentry_hash(name);

	for (int i = 0; i < DENTPERBLK; i++) {
		if (block->dents[i].inum && block->dents[i].name_hash == name_hash) {
			if (strcmp(name, block->ext[i].name) == 0)
				return &block->dents[i];
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

	for (int i = 0; i < NBLOCKS - 1; i++) {
		if (dir->data.blocks[i]) {
			dent = find_dent_in_block(dir->data.blocks[i], name);
			if (dent)
				return dent;
		}
	}
	return NULL;
}

/**
 * Update the . and .. entries in a directory inode.
 */
static void update_dir_inode(struct inode *dir, struct inode *parent)
{
	if (dir->type != T_DIR)
		return;

	if (inode_is_inline_dir(dir)) {
		dir->data.inline_dir.p_inum = inum(parent);
	} else {
		struct dentry *dot = lookup_dent(dir, ".");
		struct dentry *dotdot = lookup_dent(dir, "..");
		dot->inum = inum(dir);
		dotdot->inum = inum(parent);
	}
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
	while (start > 0 && pathname[start - 1] != '/')
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
	const char *pathname_cpy = pathname;
	char component[DENTRYNAMELEN + 1] = { '\0' };

#ifdef PCACHE_ENABLED
	if ((dent = pcache_lookup(pathname, pi)))
		return dent;
#endif  // PCACHE_ENABLED

	prev = get_root_inode();

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
			prev = dentry_get_inode(dent);
	}
	// pr_debug("\n\n");
	if (pi)
		*pi = prev;
	
#ifdef PCACHE_ENABLED
	pcache_put(pathname_cpy, dent, prev);
#endif  // PCACHE_ENABLED

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
 * Basic version of the POSIX rename system call.
 * 
 * WARNING: Not compatible with path cache. There is no mechanism
 * with which to invalidate PCACHE entries at this point.
 * 
 * WARNING: This needs to be fixed after the inline directory date.
 */
int fs_rename(const char *oldpath, const char *newpath)
{
	struct dentry *olddent = NULL, *newdent = NULL;
	struct inode *olddir = NULL, *newdir = NULL;
	struct inode *inode;

	// Old path must exist; new path will be overwritten if exists,
	// but new path's parent directory must be present.
	if (!dir_lookup(oldpath, &olddir) || !olddir) {
		return -ENOFOUND;
	}
	inode = dentry_get_inode(olddent);
	newdent = dir_lookup(newpath, &newdir);
	if (!newdir)
		return -ENOFOUND;

	// If new path doesn't exist, create it.
	if (!newdent) {
		newdent = new_dentry(newdir, dentry_get_inode(olddent), dentry_get_name(olddent));
	} else {
		newdent = olddent;
	}

	// If the inode moved is a directory, update its . and .. entries.
	if (inode->type == T_DIR) {
		update_dir_inode(inode, newdir);
	}
	return 0;
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
	if ((dentry_get_inode(dent))->type == T_DIR)
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
	if (off < 0 || off > dentry_get_inode(openfiles[fd].f_dentry)->size)
		return -EINVAL;

	openfiles[fd].offset = off;
	return off;
}

/**
 * Get the actual address for the offset in the given file.
 * 
 * @param file	The file inode.
 * @param off	The desired offset
 * @param alloc	TRUE if want block allocation if currently unused.
 * @return		The address corresponding to offset.
 */
static char *get_off_addr(struct inode *file, unsigned int off, int alloc)
{
	char *addr = NULL;
	int b = off / BSIZE;
	if (file->data.blocks[b]) {
		addr = BLKADDR(file->data.blocks[b]) + off % BSIZE;
	} else {
		if (alloc) {
			file->data.blocks[b] = alloc_data_block();
			if (file->data.blocks[b]) {
				addr = BLKADDR(file->data.blocks[b]) + off % BSIZE;
			}
		}
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
		if (!(start = get_off_addr(file, off, 0)))
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

	struct inode *file = dentry_get_inode(openfiles[fd].f_dentry);
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
 * Write to a file.
 * 
 * @param file	The file's inode
 * @param off	The starting offset
 * @param buf	Buffer wherein bytes to be written are stored
 * @param n		Number of bytes left to be written
 * @return		The number of bytes written
 */
unsigned int do_write(struct inode *file, unsigned int *off, 
					  void *buf, unsigned int n)
{
	int nwritten = 0;
	int size;	// size of each write
	char *start;

	while (n > 0) {
		if (!(start = get_off_addr(file, *off, 1)))
			goto out;

		// determine what the copy size should be
		size = n;
		if (size > BSIZE - *off % BSIZE)
			size = BSIZE - *off % BSIZE;
		memcpy(start, buf + nwritten, size);
		n -= size;
		nwritten += size;
		*off += size;
	}

out:
	return nwritten;
}

/**
 * Basic version of the POSIX write system call.
 * 
 * @param fd	File descriptor to write to
 * @param buf	Buffer wherein bytes to write are stored
 * @param count	Number of bytes requested
 * @return		Number of bytes actually written
 */
unsigned int fs_write(int fd, void *buf, unsigned int count)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (!buf)
		return -1;	

	struct inode *file = dentry_get_inode(openfiles[fd].f_dentry);
	if (file->type != T_REG)
		return -EINVTYPE;

	int ret;
	unsigned int off = openfiles[fd].offset;
	ret = do_write(file, &off, buf, count);
	
	pr_debug("write: %d bytes written.\n", ret);

	if (ret) {
		openfiles[fd].offset = off;
		if (off > file->size)
			file->size = off;
	}
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
	if (dentry_get_inode(dent)->type == T_DIR)
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
	struct inode *inode = dentry_get_inode(olddent);
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
	if (!new_dentry(dir, inode, filename)) {
		return -EALLOC;
	} else {
		return 0;
	}
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
		if (dents[i].inum) {
			if (strcmp(dentry_get_name(&dents[i]), ".") == 0)
				continue;
			if (strcmp(dentry_get_name(&dents[i]), "..") == 0)
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
	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			if (!dir_block_isempty(dir->data.blocks[i]))
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
	if (dentry_get_inode(dent)->type != T_DIR)
		return -EINVTYPE;
	if (!dir_isempty(dentry_get_inode(dent)))
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
	size_t fs_size;
	int fs_is_new = 0;
	int fd;

	if (fs) {
		printf("Error: file system is already mounted.\n");
		return -1;
	}

	if (size > MAXFSSIZE) {
		printf("Error: file system size cannot be greater than 1GB.\n");
		return -1;
	}

	if (access("fs.img", F_OK) != -1) {
		fd = open("fs.img", O_RDWR);
	} else {
		fd = open("fs.img", O_RDWR | O_CREAT, 0666);
		fs_is_new = 1;
	}
	if (!fd) {
		perror("open");
		return -1;
	}

	// If using an existing file system image, mmap the entire file.
	// Otherwise, create a file system of the desired size.
	if (!fs_is_new) {
		struct stat statbuf;
		if (fstat(fd, &statbuf) < 0) {
			perror("stat");
			return -1;
		}
		fs_size = statbuf.st_size;
	} else {
		printf("Creating new file system...\n");
		ftruncate(fd, 0);
		lseek(fd, size, SEEK_SET);
		write(fd, "\0", 1);
		fs_size = size;
	}

	fs = mmap(NULL, fs_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!fs) {
		perror("mmap");
		return -1;
	}

	// If file system is newly created, initialise everything.
	if (fs_is_new) {
		if (init_fs(fs_size) < 0)
			return -1;
	}

	// Initialize PCACHE/DCACHE if enabled.
	if (init_caches() < 0)
		return -1;

	init_fd();
	read_sb();
	close(fd); 

	pr_debug("File system successfully mounted.\n");
	return 0;
}

/**
 * Unmounts the file system.
 */
int fs_unmount(void)
{
	if (!fs)
		return -1;

#ifdef DCACHE_ENABLED
	dentry_cache_free_all();
#endif  // DCACHE_ENABLED
#ifdef PCACHE_ENABLED
	pcache_free();
#endif  // PCACHE_ENABLED
	munmap(fs, sb->size * BSIZE);
	fs = NULL;
	return 0;
}
