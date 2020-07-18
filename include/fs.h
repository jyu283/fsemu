/**
 * fsemu/include/fs.h
 * 
 * The Hobbit File System (HFS)
 * 
 * This user-space pseudo-file system is thus named in the hopes that
 * it will bear the same qualities as the Hobbits of the Shire -- simple
 * and peaceful, yet buried deep in its heart is the capacity to change
 * the world.
 * 
 * Eighteenth of July, the Year Two Thousand and Twenty of the Fifth Age
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
#include <stdbool.h>
#include <stdio.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x1000      // block size = 4096 bytes 
#define DENTCACHESIZE	64

#define INOPERBLK   (BSIZE / sizeof(struct hfs_inode))
#define BLKADDR(x)	((void *)(fs + x * BSIZE))

/* In-memory caches:
 * DCACHE: Dentry cache. Stores contents of directories.
 * 
 * Uncomment the following macros to enable the corresponding features.
 */
// #define DCACHE_ENABLED

/*
 * Simple File System layout diagram:
 * | Superblock | Inodes | Bitmap | Data |
 */

#define T_UNUSED	0
#define T_REG		1
#define T_DIR		2
#define T_DEV		3
#define NBLOCKS		15

/* Inode flags */
#define I_INLINE	0x00000001

struct hfs_dentry {
	uint32_t	inum;
	uint16_t	reclen;
	uint8_t		namelen;
	uint8_t		file_type;
	char		name[0];
};

struct hfs_inode {
	uint32_t		nlink;
	uint32_t		size;
	uint32_t		flags;
	union {
		uint32_t	blocks[NBLOCKS];
		struct {
			uint32_t				p_inum;
			struct hfs_dentry		dent_head;
		} inline_dir;
	} data;
	uint8_t			type; 
};

// For loop to traverse through INLINE directories
// d:	a hfs_dentry pointer
// dir:	pointer to an INLINE directory inode
#define for_each_inline_dent(d, dir) \
	for (d = &dir->data.inline_dir.dent_head;\
		 (char *)d < (char *)&dir->data + \
		 		sizeof(dir->data) - sizeof(struct hfs_dentry);\
		 d = (struct hfs_dentry *)((char *)d + d->reclen))

// For loop to traverse through directory file block
// d:	a hfs_dentry pointer
// b:	pinter to the start of a directory file block
#define for_each_block_dent(d, b) \
	for (d = (struct hfs_dentry *)b;	\
		 (char *)d + sizeof(struct hfs_dentry) < (char *)b + BSIZE; \
		 d = (struct hfs_dentry *)((char *)d + d->reclen))


static inline bool inode_is_inline_dir(struct hfs_inode *inode)
{
	return (inode->flags & I_INLINE);
}

static inline uint16_t get_dentry_reclen_from_name(const char *name)
{
	return (sizeof(struct hfs_dentry) + strlen(name) + 1);
}

static inline bool inline_dent_can_fit(struct hfs_inode *dir, 
										struct hfs_dentry *dent,
										uint8_t reclen)
{
	return ((char *)dent + reclen < (char *)&dir->data + sizeof(dir->data));
}

#define ROOTINO		1

#define DENTRYNAMELEN	255  // Just like in EXT2 and EXT4 

/**
 * Getter for dentry name.
 */
static inline char *dentry_get_name(struct hfs_dentry *dent)
{
	return dent->name;
}

/**
 * Setter for inline dentry name.
 */
static inline void dentry_set_name(struct hfs_dentry *dent, const char *name)
{
	uint8_t namelen = strlen(name) + 1;
	dent->namelen = namelen;
	strcpy(dent->name, name);
}

struct hfs_superblock {
	uint64_t		size;       // total size in blocks
	uint64_t		ninodes;    // number of inodes
	uint64_t		datastart;	// data start block
	uint64_t		nblocks;    // number of data blocks
	uint64_t		inodestart;	// start of inodes
	uint64_t		bitmapstart;	// start of bitmap
};

extern char *fs;
extern struct hfs_superblock *sb;
extern struct hfs_inode *inodes;
extern char *bitmap;

static inline struct hfs_inode *get_root_inode(void)
{
	return &inodes[ROOTINO];
}

static inline struct hfs_inode *inode_from_inum(uint32_t inum)
{
	return &inodes[inum];
}

static inline struct hfs_inode *dentry_get_inode(struct hfs_dentry *dent)
{
	return inode_from_inum(dent->inum);
}

struct hfs_dentry *lookup(const char *pathname);
struct hfs_dentry *dir_lookup(const char *pathname, struct hfs_inode **pi);

static inline int inum(struct hfs_inode *i)
{
	return (i - inodes);
}

#endif  // __FS_H__
