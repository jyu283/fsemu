#include "fs.h"
#include "fsemu.h"

#include <string.h>
#include <stdint.h>

static struct superblock *sb;

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
	sb->data = fs + BSIZE + inode_blocks * BSIZE + bitmap_blocks * BSIZE;
	sb->nblocks = nblocks;
	pr_debug("Allocated %d data blocks at %p.\n", nblocks, sb->data);
}

/*
 * Locates an unused (T_UNUSED) inode from the inode pool
 * and returns a pointer to that inode. Also initializes 
 * the type field.
 */
static struct inode *alloc_inode(uint8_t type)
{
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
 * Initializes the root directory by allocating an inode.
 */
static void init_rootdir(void)
{
	strcpy(sb->rootdir.name, "/");
	struct inode *rootino = alloc_inode(T_DIR);     
	sb->rootdir.inode = rootino;
}

/*
 * Master function to initialize the file system.
 */
void init_fs(size_t size)
{
	init_superblock(size);
	init_rootdir();
	pr_debug("File system initialization completed!\n");
}