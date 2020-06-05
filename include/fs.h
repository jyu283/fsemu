/**
 * fsemu/include/fs.h
 * 
 * Defines the parameters and structures of the file system.
 * For now this remains a very basic file system, no more
 * complicated than the one found in the xv6 kernel. 
 * Future updates will slowly transform it (hopefully) into
 * a much more sophisticated file system.
 */

#ifndef __FS_H__
#define __FS_H__

#include "fsemu.h"
#include "file.h"

#include <string.h>
#include <stdint.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x1000      // block size = 512 bytes
#define DENTCACHESIZE	64

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
	uint32_t		nlink;
	uint32_t		size;
	uint32_t		data[NADDR];
	uint8_t			type; 
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

extern char *fs;
extern struct superblock *sb;

struct dentry *lookup(const char *pathname);
struct dentry *dir_lookup(const char *pathname, struct inode **pi);

#endif  // __FS_H__