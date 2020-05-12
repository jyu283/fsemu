#ifndef __FS_H__
#define __FS_H__

#include <string.h>
#include <stdint.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x200      // block size = 512 bytes
#define INOPERBLK   (BSIZE / sizeof(struct inode))

/*
 * Simple File System layout diagram:
 * | Superblock | Inodes | Bitmap | Data |
 */

#define T_UNUSED    0
#define T_REG       1
#define T_DIR       2
#define T_DEV       3
#define NADDR       16

struct inode {
    uint8_t        type; 
    uint32_t       data[NADDR];
};

#define DENTRYNAMELEN   (64 - sizeof(struct inode *))

struct dentry {
    struct inode    *inode;
    char            name[DENTRYNAMELEN];
};

struct superblock {
    uint64_t        size;       // total size in blocks
    uint64_t        ninodes;    // number of inodes
    uint64_t        nblocks;    // number of data blocks
    struct inode    *inodes;
    char            *bitmap;
    char            *data;
    struct dentry   rootdir;
};

extern char *fs;

void init_fs(size_t size);

#endif  // __FS_H__