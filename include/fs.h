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
#include <time.h>
#include <pthread.h>

#define MAXFSSIZE   0x40000000
#define BSIZE       0x1000      // block size = 4096 bytes 
#define DENTCACHESIZE	64

#define INOPERBLK   (BSIZE / sizeof(struct hfs_inode))
#define BLKADDR(x)	((void *)(fs + x * BSIZE))

// Uncomment the following macros to enable the corresponding features.
#define _HFS_INLINE_DIRECTORY
// #define _HFS_DIRHASH

/*
 * Simple File System layout diagram:
 * | Superblock | Inodes | Bitmap | Data |
 */

#define T_UNUSED	0
#define T_REG		1
#define T_DIR		2
#define T_DEV		3
#define T_SYM		4
#define NBLOCKS		15

/* Inode flags */
#define I_INLINE	0x00000001		/* Inline directory/symlink */
#define I_DIRHASH	0x00000002		/* Dirhashed directory */

struct hfs_dentry {
	uint32_t	inum;
	uint16_t	reclen;
	uint8_t		namelen;
	uint8_t		file_type;
	char		name[0];
};

#define INODE_BLOCKS_SIZE	(sizeof(uint32_t) * NBLOCKS)

struct hfs_inode {
	uint32_t		nlink;
	uint32_t		size;
	uint8_t			type;
	uint32_t		flags;
	union {
		uint32_t	blocks[NBLOCKS];

		/**
		 * Dirhashed directory: If dirhash is enabled for a directory,
		 * there can only be one block full of directory entries.
		 * After the single block address, the dirhash_rec field is used
		 * to identify the dirhash table for this directory.
		 */
		struct {
			uint32_t	block;
			uint32_t	seqno;
			uint16_t	id;
		} dirhash_rec;

		/**
		 * Inline directory mode: The 60-byte hfs_inode.data field
		 * is used to directory store directory entries, as well as
		 * the parent directory's inum.
		 */
		struct {
			uint32_t				p_inum;
			struct hfs_dentry		dent_head;
		} inline_dir;

		/**
		 * Inline symbolic link path.
		 */
		char		symlink_path[INODE_BLOCKS_SIZE]; 
	} data;

	/**
	 * Last CHANGE time. Updated when inode metadata is updated.
	 */
	time_t	ctime;

	/**
	 * Last ACCESS time. Updated when inode data is last accessed.
	 */
	time_t	atime;

	/**
	 * Last MODIFY time. Updated when inode data content is changed.
	 */
	time_t	mtime;
};


#ifdef _HFS_INLINE_DIRECTORY
// For loop to traverse through INLINE directories
// d:	a hfs_dentry pointer
// dir:	pointer to an INLINE directory inode
#define for_each_inline_dent(d, dir) \
	for (d = &dir->data.inline_dir.dent_head;\
		 (char *)d < (char *)&dir->data + \
		 		sizeof(dir->data) - sizeof(struct hfs_dentry);\
		 d = (struct hfs_dentry *)((char *)d + d->reclen))

static inline bool inode_is_inline_dir(struct hfs_inode *inode)
{
	return (inode->flags & I_INLINE);
}

static inline bool inline_dent_can_fit(struct hfs_inode *dir, 
										struct hfs_dentry *dent,
										uint8_t reclen)
{
	return ((char *)dent + reclen < (char *)&dir->data + sizeof(dir->data));
}
#endif  // _HFS_INLINE_DIRECTORY


// For loop to traverse through directory file block
// d:	a hfs_dentry pointer
// b:	pinter to the start of a directory file block
#define for_each_block_dent(d, b) \
	for (d = (struct hfs_dentry *)b;	\
		 (char *)d + sizeof(struct hfs_dentry) < (char *)b + BSIZE; \
		 d = (struct hfs_dentry *)((char *)d + d->reclen))


static inline uint16_t get_dentry_reclen_from_name(const char *name)
{
	return (sizeof(struct hfs_dentry) + strlen(name) + 1);
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
	uint64_t		inode_used;	// number of inodes in use
	uint64_t		inline_inodes;	// number of inline inodes
	uint64_t		ndirectories;	// number of directories
	uint64_t		nfiles;			// number of files
	uint64_t		datastart;	// data start block
	uint64_t		nblocks;    // number of data blocks
	uint64_t		inodebitmapstart;	// start of inode bitmap
	uint64_t		inodestart;	// start of inodes
	uint64_t		bitmapstart;	// start of bitmap
	time_t			creation_time;	// creation time of file system
	time_t			last_mounted;	// last mount time
	struct hfs_dentry	rootdir;	// nameless dentry for root.
};

extern char *fs;
extern struct hfs_superblock *sb;
extern struct hfs_inode *inodes;
extern char *inobitmap, *bitmap;

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

#define MAXOPENFILES    32

/* 
 * Plan to simulate processes with threads. By which time
 * things like the openfiles array should be a property of
 * each individual process. Right now we consider the entire
 * emulator as a single process -- which of course it is but
 * in a difference sense.
 */

extern struct file openfiles[MAXOPENFILES];

/**
 * Current working directory.
 */
extern struct hfs_dentry *cwd;

#endif  // __FS_H__
