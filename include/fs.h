#ifndef __FS_H__
#define __FS_H__

#include "fsemu.h"

#include <string.h>
#include <stdint.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x200      // block size = 512 bytes
#define INOPERBLK   (BSIZE / sizeof(struct inode))
#define BLKADDR(x)	((void *)(fs + x * BSIZE))

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
	uint8_t			type; 
	uint8_t			refcount;
	uint32_t		data[NADDR];
};

#define DENTRYNAMELEN   (32 - sizeof(struct inode *))
#define DENTPERBLK		(BSIZE / sizeof(struct dentry))

struct dentry {
	struct inode	*inode;
	char			name[DENTRYNAMELEN];
};

struct superblock {
	uint64_t		size;       // total size in blocks
	uint64_t		ninodes;    // number of inodes
	uint64_t		datastart;	// data start block
	uint64_t		nblocks;    // number of data blocks
	struct inode	*inodes;	// start of inodes
	char			*bitmap;	// start of bitmap
	struct dentry	rootdir;	// root dentry
};

extern struct superblock *sb;

#endif  // __FS_H__