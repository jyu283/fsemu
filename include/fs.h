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
#define BSIZE       0x1000      // block size = 4096 bytes 
#define DENTCACHESIZE	64

#define INOPERBLK   (BSIZE / sizeof(struct inode))
#define BLKADDR(x)	((void *)(fs + x * BSIZE))

/* In-memory caches:
 * PCACHE: Pathname cache. Stores full pathname lookups.
 * DCACHE: Dentry cache. Stores contents of directories.
 * 
 * Uncomment the following macros to enable the corresponding features.
 */
// #define PCACHE_ENABLED
// #define DCACHE_ENABLED

/*
 * Simple File System layout diagram:
 * | Superblock | Inodes | Bitmap | Data |
 */

#define T_UNUSED	0
#define T_REG		1
#define T_DIR		2
#define T_DEV		3
#define NDIR		16

struct inode {
	uint32_t		nlink;
	uint32_t		size;
	uint32_t		data[NDIR];
	uint8_t			type; 
};

#define DENTRYSIZE		256
#define DENTRYNAMELEN   (DENTRYSIZE - sizeof(int))
#define DENTPERBLK		(BSIZE / sizeof(struct dentry))

struct dentry {
	int				inum;
	char			name[DENTRYNAMELEN];
};

struct superblock {
	uint64_t		size;       // total size in blocks
	uint64_t		ninodes;    // number of inodes
	uint64_t		datastart;	// data start block
	uint64_t		nblocks;    // number of data blocks
	uint64_t		inodestart;	// start of inodes
	uint64_t		bitmapstart;	// start of bitmap
	struct dentry	rootdir;	// root dentry
};

extern char *fs;
extern struct superblock *sb;
extern struct inode *inodes;
extern char *bitmap;

struct dentry *lookup(const char *pathname);
struct dentry *dir_lookup(const char *pathname, struct inode **pi);

static inline int inum(struct inode *i)
{
	return (i - inodes);
}

static inline struct inode *dentry_get_inode(struct dentry *dent)
{
	return &inodes[dent->inum];
}


#endif  // __FS_H__