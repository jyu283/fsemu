#include "fs.h"
#include "fs_syscall.h"
#include "fsemu.h"

#include <string.h>
#include <stdint.h>

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
			return inode;
		}
	}
	return NULL;
}

/*
 * Consult the bitmap and allocate a datablock.
 */
static uint64_t alloc_data_block(void)
{
	char *bm = sb->bitmap;
	for (int i = 0; i < sb->size; i++) {
		if (bm[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (((bm[i] >> j) & 1) == 0) {
					bm[i] |= 1 << j;
					return (sb->datastart + (i * 8 + j));
				}
			}
		}
	}
	return -1;
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

	// If no unused blocks left or allocation function failed.
	if (unused == -1 || (dir->data[unused] = alloc_data_block()) < 0) {
		printf("Error: data allocation failed\n");
		return NULL;
	}

	return (struct dentry *)BLKADDR(dir->data[unused]);
}

static inline void init_dent(struct dentry *dent, struct inode *inode,
							const char *name)
{
	dent->inode = inode;
	inode->nlink++;
	strcpy(dent->name, name);
}

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
 * 
 * This would have been a wonderful opportunity to correct
 * Ken Thompson's "mistake" and name this function create().
 * But I decided against it.
 */
int creat(struct inode *dir, const char *name, uint8_t type)
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
void init_fs(size_t size)
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
 * Basic version of the POSX close system call.
 */
int fs_close(int fd)
{
	if (fd < 0 || fd >= MAXOPENFILES)
		return -1;

	openfiles[fd].f_dentry = NULL;
	openfiles[fd].offset = 0;
	return 0;
}

/* 
 * Debug function:
 * Create a file/directory/device at root directory.
 */
int db_creat_at_root(const char *name, uint8_t type)
{
	return creat(sb->rootdir.inode, name, type);
}

/*
 * Debug function:
 * Create a directory inode at root directory.
 */
int db_mkdir_at_root(const char *name)
{
	return creat(sb->rootdir.inode, name, T_DIR);
}