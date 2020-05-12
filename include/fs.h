#ifndef __FS_H__
#define __FS_H__

#include <string.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x200      // block size = 512 bytes
#define INOPERBLK   (BSIZE / sizeof(struct inode))

#define DENTRYNAMELEN   16

/*
 * Simple File System layout diagram:
 * | Superblock | Inodes | Bitmap | Data |
 */

struct inode {
    unsigned int data[16];
};

struct dentry {
    struct inode    *inode;
    char            name[DENTRYNAMELEN];
};

struct superblock {
    unsigned long   size;       // total size in blocks
    unsigned long   ninodes;    // number of inodes
    unsigned long   nblocks;    // number of data blocks
    struct inode    *inodes;
    char            *bitmap;
    char            *data;
};

extern char *fs;

void init_superblock(size_t size);

#endif  // __FS_H__