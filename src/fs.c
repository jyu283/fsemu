#include "fs.h"
#include "fsemu.h"

#include <string.h>

void init_superblock(size_t size)
{
    struct superblock *sb = (struct superblock *)fs;

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