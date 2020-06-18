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
#include <stdio.h>

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

#define ROOTINO		1

#define DENTRYNAMELEN	255  // Just like in EXT2 and EXT4 
#define DENTRYSIZE		(sizeof(struct dentry) + sizeof(struct dentry_ext))
#define DENTPERBLK		(BSIZE / DENTRYSIZE)

/**
 * Split dentry:
 * Each directory block will be split into two sections:
 * The first is a compact lookup table with hash values. (struct dentry)
 * The second (struct dentry_ext) contains the full file names as well 
 * as the inum of the corresponding directory entry.
 */

struct dentry {
	unsigned long	name_hash;
	int				inum;
};

struct dentry_ext {
	char			name[DENTRYNAMELEN];
};

struct dentry_block {
	struct dentry		dents[DENTPERBLK];
	struct dentry_ext	ext[DENTPERBLK];
};

/**
 * Hash function to obtain name_hash in struct dentry.
 */
static unsigned long dentry_hash(const char *name)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *name++))
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c

    return hash;
}

/**
 * Obtain the (detached) extended section of a dentry.
 * 
 * Exploits the fact that blocks are 4K aligned (at least I damn well
 * hope so) to calculate where the extended part corresponding to a
 * dentry should be.
 */
static inline struct dentry_ext *dentry_get_ext(struct dentry *dent)
{
	struct dentry_block *block = (void *)dent - ((unsigned long)dent % BSIZE);
	int off = ((unsigned long)dent - (unsigned long)block) / sizeof(struct dentry);
	return &block->ext[off];
}

static inline char *dentry_get_name(struct dentry *dent)
{
	return (char *)&dentry_get_ext(dent)->name;
}

struct superblock {
	uint64_t		size;       // total size in blocks
	uint64_t		ninodes;    // number of inodes
	uint64_t		datastart;	// data start block
	uint64_t		nblocks;    // number of data blocks
	uint64_t		inodestart;	// start of inodes
	uint64_t		bitmapstart;	// start of bitmap
};

extern char *fs;
extern struct superblock *sb;
extern struct inode *inodes;
extern char *bitmap;

static inline struct inode *get_root_inode(void)
{
	return &inodes[ROOTINO];
}

static inline struct inode *dentry_get_inode(struct dentry *dent)
{
	return &inodes[dent->inum];
}

struct dentry *lookup(const char *pathname);
struct dentry *dir_lookup(const char *pathname, struct inode **pi);

static inline int inum(struct inode *i)
{
	return (i - inodes);
}

#endif  // __FS_H__
