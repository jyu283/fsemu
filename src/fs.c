/**
 * fsemu/src/fs.c
 * 
 * The core of the file system. For now this is where all the 
 * file system-related system calls are defined.
 */

#include "fs.h"
#include "fs_syscall.h"
#include "fserror.h"
#include "util.h"
#include "fsemu.h"
#include "file.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

/* Optional add-on features. */
#ifdef _HFS_DIRHASH
#include "dirhash.h"
#endif

char *fs = NULL;
struct hfs_superblock *sb;
struct hfs_inode *inodes;
char *inobitmap, *bitmap;

/**
 * File descriptors.
 */
struct file openfiles[MAXOPENFILES];

/**
 * Current working directory.
 */
struct hfs_dentry *cwd;

#ifdef _HFS_INLINE_DIRECTORY
static inline void inode_set_inline_flag(struct hfs_inode *inode)
{
	inode->flags |= I_INLINE;
	sb->inline_inodes++;
}

static inline void inode_unset_inline_flag(struct hfs_inode *inode)
{
	inode->flags &= (~I_INLINE);
	sb->inline_inodes--;
}
#endif


static inline void inode_touch_atime(struct hfs_inode *inode)
{
	inode->atime = time(NULL);
}

static inline void inode_touch_ctime(struct hfs_inode *inode)
{
	inode->ctime = time(NULL);
}

static inline void inode_touch_mtime(struct hfs_inode *inode)
{
	inode->mtime = time(NULL);
}

/*
 * Calculate the positions of each region of the file system.
 *  - Superblock is the very first block in the file system.
 *  - Inode region should be aboud 3% of the entire file system.
 *  - Bitmap maps all the blocks in the file system.
 *  - The remaining are data blocks.
 */
static void init_superblock(size_t size)
{
	sb = (struct hfs_superblock *)fs;
	memset(sb, 0x0, BSIZE);

	int total_blocks = size / BSIZE;
	int inode_blocks = total_blocks * 0.03;
	sb->size = total_blocks;
	pr_info("Total blocks: %d.\n", total_blocks);

	sb->inodebitmapstart = 1;
	sb->ninodes = inode_blocks * INOPERBLK; 
	int inode_bitmap_bytes = sb->ninodes / 8;
	int inode_bitmap_blocks = inode_bitmap_bytes / BSIZE + 1;
	sb->inodestart = sb->inodebitmapstart + inode_bitmap_blocks;

	sb->bitmapstart = sb->inodestart + inode_blocks;

	// Here, number of data blocks are overestimated for convenience
	int bitmap_blocks = (total_blocks / 8) / BSIZE;

	sb->datastart = sb->bitmapstart + bitmap_blocks + 1;
	int nblocks = total_blocks - sb->datastart + 1;
	sb->nblocks = nblocks;

	inodes = BLKADDR(sb->inodestart);
	bitmap = BLKADDR(sb->bitmapstart);
	inobitmap = BLKADDR(sb->inodebitmapstart);

	sb->creation_time = (uint64_t)time(NULL);
}

/**
 * Initialise bitmaps.
 */
static inline void init_bitmaps(void)
{
	// set inode #0 so root inode will be allocated to 1.
	inobitmap[0] |= 1;
}

/**
 * Read from an existing superblock to populate the global variables.
 */
static inline void read_sb(void)
{
	sb = (struct hfs_superblock *)fs;
	inodes = BLKADDR(sb->inodestart);
	inobitmap = BLKADDR(sb->inodebitmapstart);
	bitmap = BLKADDR(sb->bitmapstart);
}

/**
 * Zero out a data block before allocating it to an inode.
 */
static void wipe_block(uint32_t blocknum)
{
	char *block = BLKADDR(blocknum);
	memset(block, 0, BSIZE);
}

/*
 * Consult the bitmap and allocate a datablock.
 * 
 * Returns 0 for failure, positive int for allocated block number.
 */
static uint32_t alloc_data_block(void)
{
	uint32_t block = 0;
	for (int i = 0; i < sb->size; i++) {
		if (bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (((bitmap[i] >> j) & 1) == 0) {
					bitmap[i] |= 1 << j;
					block = sb->datastart + (i * 8 + j);
					wipe_block(block);
					goto out;
				}
			}
		}
	}
out:
	return block;
}

/**
 * Free data block number b.
 */
static void free_data_block(uint32_t b)
{
	char *bm = BLKADDR(sb->bitmapstart);
	int byte = b / 8;
	int bit = b % 8;
	bm[byte] &= ~(1 << bit);
}

/**
 * Initialize inode, set type and flags.
 */
static void init_inode(struct hfs_inode *inode, uint8_t type)
{
	memset((void *)inode, 0x0, sizeof(struct hfs_inode));
	inode->type = type;
	sb->inode_used++;
	// Enable inline by default for directories.
	if (type == T_DIR) {
		sb->ndirectories++;
#ifdef _HFS_INLINE_DIRECTORY
		inode_set_inline_flag(inode);
#endif
#ifdef _HFS_DIRHASH
		// Enable dirhash if and only if the directory
		// is not set to inline mode.
		if (!(inode->flags & I_INLINE))
			inode->flags |= I_DIRHASH;
#endif
	} else if (type == T_REG) {
		sb->nfiles++;
	}
	inode_touch_atime(inode);
	inode_touch_mtime(inode);
	inode_touch_ctime(inode);
}

/**
 * Find a free inode from the inode bitmap and return its inum.
 */
static int get_free_inum(void)
{
	int inum = 0;
	for (int i = 0; i < sb->ninodes / 8; i++) {
		if (inobitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (((inobitmap[i] >> j) & 1) == 0) {
					inobitmap[i] |= 1 << j;
					inum = i * 8 + j;
					goto out;
				}
			}
		}
	}
out:
	return inum;
}

/*
 * Locates an unused (T_UNUSED) inode from the inode pool
 * and returns a pointer to that inode. Also initializes 
 * the type field.
 */
static struct hfs_inode *alloc_inode(uint8_t type)
{
	int inum;
	if (!(inum = get_free_inum()))
		return NULL;
	else {
		struct hfs_inode *inode = inode_from_inum(inum);
		init_inode(inode, type);
		return inode;
	}
}

/**
 * Frees an inode and all the data blocks it uses.
 * 
 * Do not call this function to remove a file!
 * Use unlink_dent instead which will call this function.
 */
static int free_inode(struct hfs_inode *inode)
{
	if (inode->nlink > 0)
		return -1;

	for (int i = 0; i < NBLOCKS; i++) {
		if (inode->data.blocks[i]) {
			free_data_block(inode->data.blocks[i]);
			inode->data.blocks[i] = 0;
		}
	}

	if (inode->type == T_DIR)
		sb->ndirectories--;
	else if (inode->type == T_REG)
		sb->nfiles--;

	inode->type = T_UNUSED;
	sb->inode_used--;

	int ino = inum(inode);
	int byte = ino / 8;
	int bit = ino % 8;
	inobitmap[byte] &= ~(1 << bit);

	return 0;
}

/**
 * Find an unused dentry spot from a block.
 */
static struct hfs_dentry *alloc_dentry_from_block(uint32_t block_num,
													uint16_t reclen)
{
	char *block = BLKADDR(block_num);
	struct hfs_dentry *dent;
	for_each_block_dent(dent, block) {
		if (dent->reclen == 0) {  // end of dentry list
			if ((char *)dent + reclen <= block + BSIZE) {
				dent->reclen = reclen;
				return dent;
			} else {
				break;
			}
		}
		if (!dent->inum && dent->reclen >= reclen) {
			return dent;
		}
	}
	return NULL;
}

/**
 * Allocates a dentry of reclen to the given directory inode.
 * 
 * _HFS_DIRHASH:
 * If, as a result of this allocation, the size of this directory grows
 * beyond one block, then we need to unset the I_DIRHASH flag to indicate
 * that this directory will NO LONGER use dirhash.
 */
static struct hfs_dentry *alloc_dentry(struct hfs_inode *dir, uint16_t reclen)
{
	struct hfs_dentry *dent;
	int unused = -1;
	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			dent = alloc_dentry_from_block(dir->data.blocks[i], reclen);
			if (dent)
				return dent;
		} else {
			if (unused == -1)
				unused = i;
		}
	}

	// No available blocks can be allocated for this inode
	if (unused < 0)
		return NULL;

	// allocate new data block to unused address
#ifdef _HFS_DIRHASH
	// If we are allocating ALL BUT the VERY FIRST block, then 
	// we need to disable dirhash for this directory.
	if (dir->data.blocks[0])
		dir->flags &= ~I_DIRHASH;
#endif 
	if ((dir->data.blocks[unused] = alloc_data_block()) == 0) {
		printf("Error: data allocation failed.\n");
		return NULL;
	}

	dir->size += BSIZE;  // increase directory size by one block
	return (struct hfs_dentry *)BLKADDR(dir->data.blocks[unused]);
}

/**
 * Initialize a dentry with a name and link it to the inode.
 */
static inline void init_dentry(struct hfs_dentry *dent, 
								struct hfs_inode *inode, const char *name)
{
	dent->inum = inum(inode);
	dent->file_type = inode->type;
	dentry_set_name(dent, name);

	// reclen is only changed when:
	//  - the dentry is first created, or when
	//  - the dentry is being coalesced with an adjacent one
	if (dent->reclen == 0)
		dent->reclen = get_dentry_reclen_from_name(name);
}

/**
 * Unlinks a dentry from its inode.
 * As a result of this unlinking, the inode's nlink is decremented.
 * 
 * NOTE: Do NOT reset the name. The rename() system call relies on this
 * function not wiping out the name.
 * 
 * TODO: Coalescing adjacent free spaces
 */
static int unlink_dent(struct hfs_dentry *dent)
{
	if (strcmp(dent->name, ".") == 0 || strcmp(dent->name, "..") == 0)
		return -1;

	struct hfs_inode *inode = dentry_get_inode(dent);
	inode->nlink--;	 // Can be done in if clause. I know. Keep quiet.
	// If inode nlink becomes 0, deallocate that inode.
	if (inode->nlink == 0) {
		free_inode(inode);
	}
	dent->inum = 0;
	return 0;
}

#ifdef _HFS_INLINE_DIRECTORY
/**
 * Create a new inline directory entry.
 */
static struct hfs_dentry *alloc_inline_dentry(struct hfs_inode *dir, 
											uint16_t reclen)
{
	struct hfs_dentry *dent;
	for_each_inline_dent(dent, dir) {
		if (dent->inum != 0)
			continue;

		// This dentry is not large enough
		if (dent->reclen != 0 && dent->reclen < reclen)
			continue;

		// This is a never-before-used dentry.
		if (dent->reclen == 0) {
			if (inline_dent_can_fit(dir, dent, reclen))
				return dent;
			else
				break;
		}
	}
	return NULL;
}

/**
 * Converts an inline directory to a regular directory.
 * 
 * First, allocate a new block for this directory, then fill in the
 * "." and ".." dentries. Finally, convert all inline entries to
 * regular entries and place them also in the block, which is then
 * attached to the inode.
 */
static int convert_inline_directory(struct hfs_inode *dir)
{
	if (!inode_is_inline_dir(dir))
		return -1;

	struct hfs_dentry *dent;
	uint32_t block = alloc_data_block();
	if (!block) 
		return -1;

	/* 
	 * The . and .. entry. Note that we call init_regular_dent() here,
	 * which would NOT increment the nlink count, which is what we want.
	 */
	// dot entry
	dent = alloc_dentry_from_block(block, get_dentry_reclen_from_name("."));
	init_dentry(dent, dir, ".");
	// dot dot entry
	dent = alloc_dentry_from_block(block, get_dentry_reclen_from_name(".."));
	init_dentry(dent, inode_from_inum(dir->data.inline_dir.p_inum), "..");

	// Conver the inline directory entries and fill in the block.
	struct hfs_dentry *inline_dent;
	for_each_inline_dent(inline_dent, dir) {
		if (!inline_dent->reclen)
			break;  // end of list
		if (!inline_dent->inum)
			continue;  // empty item
		dent = alloc_dentry_from_block(block, inline_dent->reclen);	
		init_dentry(dent, &inodes[inline_dent->inum], inline_dent->name);
	}

	// Unset the inline bit in flag
	inode_unset_inline_flag(dir);

#ifdef _HFS_DIRHASH
	dir->flags |= I_DIRHASH;
#endif

	// Wipe out the inode.data area, and set the first block
	// Thus dawns a new age for this directory inode.
	memset(&dir->data, 0x0, sizeof(dir->data));
	dir->data.blocks[0] = block;
	return 0;
}
#endif  // _HFS_INLINE_DIRECTORY

/**
 * Find an unused dentry in dir and initialize it with
 * a name and an inode.
 */
static struct hfs_dentry *new_dentry(struct hfs_inode *dir, 
								struct hfs_inode *inode, const char *name)
{
	struct hfs_dentry *dent = NULL;
	uint16_t reclen = get_dentry_reclen_from_name(name);
#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		// If unable to allocate inline, convert directory inode
		// to regular mode.
		if (!(dent = alloc_inline_dentry(dir, reclen)))
			convert_inline_directory(dir);
	}
#endif  // _HFS_INLINE_DIRECTORY

	// Not inline or inline allocation was unsuccessful
	if (!dent) {
		if (!(dent = alloc_dentry(dir, reclen)))
			return NULL;
	}

	init_dentry(dent, inode, name);
	inode->nlink++;

#ifdef _HFS_DIRHASH
	if (dir->flags & I_DIRHASH)
		hfs_dirhash_put(dir, dent);
#endif

	return dent;
}

/*
 * Create the . and .. dentries for a directory inode.
 * 
 * OR: If inline directories are enabled, simply use the first 
 * 4 bytes of inode.data to store the parent dir's inum.
 */
static int init_dir_inode(struct hfs_inode *dir, struct hfs_inode *parent)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		dir->data.inline_dir.p_inum = inum(parent);
		dir->data.inline_dir.dent_head.inum = 0;

		// In a regular directory we would create the . and ..
		// entries, thereby incrementing the two directories'
		// nlink count. Here, we'll have to fake that.
		dir->nlink++;     // for the would-have-been "." entry
		parent->nlink++;  // for the would-have-been ".." entry

		return 0;
	}
#endif  // _HFS_INLINE_DIRECTORY

	// Regular directory
	if (!new_dentry(dir, dir, ".")) {
		printf("Error: failed to create . dentry.\n");
		return -EALLOC;
	}
	if (!new_dentry(dir, parent, "..")) {
		printf("Error: failed to create .. dentry.\n");
		return -EALLOC;
	}
	return 0;
}

/*
 * Create a file/directory/device.
 */
struct hfs_dentry *do_creat(struct hfs_inode *dir, 
							const char *name, uint8_t type)
{
	int ret;
	struct hfs_dentry *dent;
	struct hfs_inode *inode;

	inode = alloc_inode(type);
	if (!inode)
		return NULL;

	if (!(dent = new_dentry(dir, inode, name)))
		return NULL;

	if (type == T_DIR) {
		if ((ret = init_dir_inode(inode, dir)) < 0) {
			unlink_dent(dent);
			return NULL;
		}
	}
	return dent;
}

/*
 * Initializes the root directory by allocating an inode.
 */
static void init_rootdir(void)
{
	struct hfs_inode *rootino = alloc_inode(T_DIR);     
	pr_info("Root inum: %d\n", inum(rootino));
	init_dir_inode(rootino, rootino);	// root inode parent is self
	sb->rootdir.file_type = T_DIR;
	sb->rootdir.inum = inum(rootino);
}

/**
 * Initializes file descriptor table.
 */
static void init_fd(void)
{
	for (int i = 0; i < MAXOPENFILES; i++) {
		openfiles[i].f_dentry = NULL;
		openfiles[i].offset = 0;
	}
}

/**
 * Master function to initialize the file system.
 */
static int init_fs(size_t size)
{
	init_superblock(size);	// fill in superblock
	init_bitmaps();
	init_rootdir();			// create root directory

	pr_debug("Inode size: %ld\n", sizeof(struct hfs_inode));
	pr_debug("Dentry size: %ld\n", sizeof(struct hfs_dentry));
	pr_info("File system initialization completed!\n");
	return 0;
}

/**
 * Initialize in-memory caches.
 */
static int init_caches(void)
{
#ifdef _HFS_DIRHASH
	hfs_dirhash_init();
#endif
	return 0;
}

/**
 * Free in-memory hashes.
 */
static void free_caches(void)
{
#ifdef _HFS_DIRHASH
	hfs_dirhash_free();
#endif
}

/**
 * Locate a dentry in a block of dentries.
 * 
 * Cache-light split dentry:
 * First check dent->name_hash to quickly dismiss non-matching dentries.
 * If name_hash is a match, then use strcmp on the full name to confirm.
 */
static struct hfs_dentry *find_dent_in_block(uint32_t bnum, const char *name)
{
	char *block = BLKADDR(bnum);
	struct hfs_dentry *dent;
	int namelen = strlen(name) + 1;

	for_each_block_dent(dent, block) {
		if (dent->reclen == 0)
			break;
		if (!dent->inum)
			continue;
		if (dent->namelen == namelen && strcmp(dent->name, name) == 0) {
			return dent;		
		}
	}

	return NULL;
}

#ifdef _HFS_INLINE_DIRECTORY
/**
 * Lookup a dentry in a given INLINE directory (inode).
 */
static struct hfs_dentry *lookup_inline_dent(struct hfs_inode *dir, 
											const char *name)
{
	struct hfs_dentry *dent;
	int namelen = strlen(name) + 1;

	for_each_inline_dent(dent, dir) {
		if (dent->reclen == 0)
			break;
		if (dent->inum == 0)
			continue;
		if (namelen == dent->namelen && strcmp(name, dent->name) == 0) {
			return dent;
		}	
	}
	return NULL;
}
#endif  // _HFS_INLINE_DIRECTORY

/**
 * Lookup a dentry in a given directory (inode).
 * Helper function for lookup() but for now it's basically all
 * lookup() does.
 */
static struct hfs_dentry *lookup_dent(struct hfs_inode *dir, const char *name)
{
#ifdef _HFS_INLINE_DIRECTORY
	// Inline directory lookup
	if (inode_is_inline_dir(dir)) {
		return lookup_inline_dent(dir, name);
	}
#endif  // _HFS_INLINE_DIRECTORY

	// Regular lookup
	struct hfs_dentry *dent = NULL;
	for (int i = 0; i < NBLOCKS - 1; i++) {
		if (dir->data.blocks[i]) {
			dent = find_dent_in_block(dir->data.blocks[i], name);
			if (dent)
				return dent;
		}
	}
	return NULL;
}

/**
 * Update the . and .. entries in a directory inode.
 */
static void update_dir_inode(struct hfs_inode *dir, struct hfs_inode *parent)
{
	if (dir->type != T_DIR)
		return;

#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		dir->data.inline_dir.p_inum = inum(parent);
		return;
	}
#endif  // _HFS_INLINE_DIRECTORY

	struct hfs_dentry *dot = lookup_dent(dir, ".");
	struct hfs_dentry *dotdot = lookup_dent(dir, "..");
	dot->inum = inum(dir);
	dotdot->inum = inum(parent);
}

/**
 * Get the next part (component) of the pathname separated by '/'.
 * The component is filled into the char array provided by lookup().
 */
static int get_path_component(const char **pathname, char *component)
{
	while (**pathname == '/')
		*pathname += 1;

	if (**pathname == '\0')
		return 0;

	int i = 0;
	while (**pathname != '/' && **pathname != '\0') {
		component[i++] = **pathname;
		*pathname += 1;
	}
	component[i] = '\0';
	return i;
}

/**
 * Check if a pathname is empty except for separators.
 */
static int path_is_empty(const char *pathname)
{
	const char *c = pathname;
	while (*c == '/')
		c++;

	// if path is empty, we should be at the end
	return (*c == '\0');
}

/**
 * Get the name of the file from the full path name.
 */
static void get_filename(const char *pathname, char *name)
{
	int start, end, len;
	
	end = strlen(pathname) - 1;
	while (pathname[end] == '/')
		end--;
	start = end;
	while (start > 0 && pathname[start - 1] != '/')
		start--;
	len = end - start + 1;
	strncpy(name, &pathname[start], len);
	name[len] = '\0';
}

/*
 * Different types/usages of lookup:
 * 
 * 1. In determining if a file exists:
 *     Need to go all the way to the last component of the pathname,
 * and return the dentry of that last component, or NULL if any component
 * along the way does not exist. 
 * 
 * 2. In creating a new file:
 *     Need to check if the file already exists (1) and then also
 * obtain a pointer to the parent directory in order to create the 
 * new file. It is okay if the last component does not exist, but not okay
 * if a component along the way does not exist.
 * 
 * Also, every component except the last should be a directory. This should
 * also be checked during lookup.  
 * 
 * Conclusion: need to know when the lookup has reached the last component
 * of the pathname, and lookup needs to somehow return the second-to-last
 * component to the caller, preferrably in the form of an inode pointer.
 * 
 */


/**
 * Handling paths containing . or ..
 * e.g. /usr/local/./src/../
 * 
 * This lookup would return the .. dentry of the src/ directory inode,
 * the pi (parent inode) pointer will poitn to src/. This is correct.
 * 
 * If src/ were an inline directory, the dentry returned would be 
 * a DUMMY dentry that does not exist in the file system, since there
 * is no actual ".." entry in an inline directory.
 * 
 * There is where VFS would make things easier, I suppose.
 */

/**
 * Dummy dentry with reserved space for a full-length name.
 */
struct hfs_dummy_dentry {
	struct hfs_dentry 	dent;
	char				name[DENTRYNAMELEN];
};

/**
 * Lookup the provided pathname. (No relative pathnames.) 
 * Even if pathname does not start with '/', the file system
 * will still use the root directory as a starting point.
 */
static struct hfs_dentry *do_lookup(const char *pathname, struct hfs_inode **pi)
{
	struct hfs_dentry *dent = NULL;
	struct hfs_dentry *prev = NULL;
	struct hfs_inode *iprev = NULL;
	const char *pathname_cpy = pathname;
	char component[DENTRYNAMELEN + 1] = { '\0' };

#ifdef _HFS_INLINE_DIRECTORY
	static struct hfs_dummy_dentry dummy_dentry;
#endif

	prev = (pathname[0] == '/') ? &sb->rootdir : cwd;

	// FIXME: lookup would fail if called with "/"
	while (get_path_component(&pathname, component)) {
		iprev = dentry_get_inode(prev);
		if (iprev->type != T_DIR) 
			return NULL;

#ifdef _HFS_INLINE_DIRECTORY
		// Inline directories require special handling with
		// the "." and ".." entries since they don't really exist.
		if (inode_is_inline_dir(iprev)) {
			if (strcmp(component, ".") == 0) {
				dent = prev;
				goto step_check;
			} else if (strcmp(component, "..") == 0) {
				dent = &dummy_dentry.dent;
				dent->file_type = T_DIR;
				dent->inum = iprev->data.inline_dir.p_inum;
				strcpy(dent->name, "..");
				dent->namelen = strlen("..") + 1;
				dent->reclen = get_dentry_reclen_from_name("..");
				goto step_check;
			}
		}
#endif
		if (iprev->flags & I_DIRHASH) {
#ifdef _HFS_DIRHASH
			struct hfs_dirhash_entry *ent;
			if ((ent = hfs_dirhash_lookup(iprev, component)))
				dent = ent->dent;
			else
				dent = NULL;
#endif
		} else {
			dent = lookup_dent(iprev, component);
		}
step_check:
		if (!dent) {
			// If lookup failed on the last component, then fill
			// in the pi field. Otherwise, set pi field to NULL
			// to indicate that lookup failed along the way.
			if (!path_is_empty(pathname))
				iprev = NULL;
			break;
		}
		if (!path_is_empty(pathname)) {
			// Continue traversal, current directory becomes new prev.
			prev = dent;
		}
	}

	if (pi)
		*pi = iprev;
	
	return dent;
}

/**
 * Regular lookup. Searches for the file specified by pathname
 * and return the resulting dentry or NULL if file isn't found.
 */
struct hfs_dentry *lookup(const char *pathname)
{
	return do_lookup(pathname, NULL);
}

/**
 * Use this lookup in system calls that involve modifying things.
 * (e.g. mkdir, rmdir, creat, etc.) 
 * 
 * @param pathname The pathname to lookup
 * @param pi dir_lookup() will store the inode of the parent dir. 
 * This will be NULL if anything but the last component in the path 
 * does not exist.
 * @return The dentry to the file in question.
 */
struct hfs_dentry *dir_lookup(const char *pathname, struct hfs_inode **pi)
{
	return do_lookup(pathname, pi);
}

/**
 * Find an unused file struct and return its index 
 * as the file descriptor.
 */
static int get_open_fd(void)
{
	for (int i = 0; i < MAXOPENFILES; i++) {
		if (!(openfiles[i].f_dentry)) {
			return i;
		}
	}
	return -ENOFD;
}

/**
 * Basic version of the POSIX creat system call.
 * Currently no support for flags/modes.
 * 
 * For onw creates a T_REG file by default.
 */
int fs_creat(const char *pathname)
{
	int fd, ret;
	struct hfs_dentry *dent;
	struct hfs_inode *dir;

	if ((dent = dir_lookup(pathname, &dir))) {
		pr_warn("%s already exists.\n", pathname);
		return -EEXISTS;
	}
	if (!dir)
		return -ENOFOUND;
	if (dir->type != T_DIR)
		return -EINVTYPE;

	char filename[DENTRYNAMELEN + 1];
	get_filename(pathname, filename);

	if (strlen(filename) > DENTRYNAMELEN)
		return -EINVNAME;

	if (!(do_creat(dir, filename, T_REG)))
		return -EALLOC;
	
	return 0;
}

/**
 * Basic version of the POSIX rename system call.
 * 
 * NOTE: Behaviour when renaming old path to an existing new path:
 *       NEW PATH SHOULD BE OVERWRITTEN.
 * 
 * Process of renaming an entry to another:
 *   Resolve newpath, delete entry if exists.
 *   Resolve oldpath, delete it.
 *   Create new entry at new location.
 * 
 * The . and .. entries cannot be removed. So if either oldpath or newpath
 * resolves to a . or .. entry, unlinking them will result in an error.
 */
int fs_rename(const char *oldpath, const char *newpath)
{
	struct hfs_dentry *olddent = NULL, *newdent = NULL;
	struct hfs_inode *olddir = NULL, *newdir = NULL;
	struct hfs_inode *inode;

	// Old path must exist; new path will be overwritten if exists,
	// but new path's parent directory must be present.
	if (!(olddent = dir_lookup(oldpath, &olddir)) || !olddir)
		return -ENOFOUND;
	if (strcmp(olddent->name, ".") == 0 || strcmp(olddent->name, "..") == 0)
		return -EINVAL;

	char newname[DENTRYNAMELEN];
	get_filename(newpath, newname);

	inode = dentry_get_inode(olddent);

	// remove entry at new path if one exists.
	newdent = dir_lookup(newpath, &newdir);
	if (!newdir)
		return -ENOFOUND;
	if (newdent) {
		if (newdent == olddent)
			return -ESAME;
		if (newdent->file_type != olddent->file_type)
			return -EINVTYPE;

		unlink_dent(newdent);
	}

	// NOTE: new_dentry may have converted directory from inline to regular,
	// so we have to unlink before creating new dentry. Since unlink_dent
	// decreases nlink and we don't need that, we'll preemptively increase
	// nlink by 1 prior to unlinking the old inode.
	inode->nlink++;		
	unlink_dent(olddent);
	new_dentry(newdir, inode, newname);
	inode->nlink--;

	if (inode->type == T_DIR)
		update_dir_inode(inode, newdir);

	inode_touch_ctime(inode);
	inode_touch_mtime(newdir);

	return 0;
}

/**
 * Basic version of the POSIX open system call.
 * Currently no support for flags/modes as there are no
 * processes yet.
 */
int fs_open(const char *pathname)
{
	int fd;
	struct hfs_dentry *dent;

	if ((fd = get_open_fd()) < 0)
		return fd;	// ENOFD
	if ((dent = lookup(pathname)) == NULL)
		return -ENOFOUND;
	if ((dentry_get_inode(dent))->type == T_DIR)
		return -EINVTYPE;

	openfiles[fd].f_dentry = dent;
	openfiles[fd].offset = 0;
	return fd;
}

/**
 * Check if a give file descriptor is actually in use.
 */
static int fd_inuse(int fd)
{
	if (fd < 0 || fd >= MAXOPENFILES)
		return -1;
	if (!openfiles[fd].f_dentry)
		return -1;
	return 1;
}

/**
 * Basic version of the POSIX close system call.
 */
int fs_close(int fd)
{
	if (!fd_inuse(fd))
		return -EINVFD;

	openfiles[fd].f_dentry = NULL;
	openfiles[fd].offset = 0;
	return 0;
}

/**
 * Basic version of the POSIX lseek system call.
 * Future versions will implement whence. (see LSEEK(2))
 */
unsigned int fs_lseek(int fd, unsigned int off)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (off < 0 || off > dentry_get_inode(openfiles[fd].f_dentry)->size)
		return -EINVAL;

	openfiles[fd].offset = off;
	return off;
}

/**
 * Get the actual address for the offset in the given file.
 * 
 * @param file	The file inode.
 * @param off	The desired offset
 * @param alloc	TRUE if want block allocation if currently unused.
 * @return		The address corresponding to offset.
 */
static char *get_off_addr(struct hfs_inode *file, unsigned int off, int alloc)
{
	int b = off / BSIZE;
	if (b > NBLOCKS)
		return NULL;

	char *addr = NULL;
	if (file->data.blocks[b]) {
		addr = BLKADDR(file->data.blocks[b]) + off % BSIZE;
	} else {
		if (alloc) {
			file->data.blocks[b] = alloc_data_block();
			if (file->data.blocks[b]) {
				addr = BLKADDR(file->data.blocks[b]) + off % BSIZE;
			}
		}
	}
	return addr;
}

/**
 * Read from a file.
 * 
 * @param file	The file's inode
 * @param off	The starting offset
 * @param buf	Buffer wherein bytes read are stored
 * @param n		Number of bytes requested
 * @return		The number of bytes read
 * 
 */
unsigned int do_read(struct hfs_inode *file, unsigned int off, 
					 void *buf, unsigned int n)
{
	int nread = 0;
	int size;	// size of each copy
	char *start;

	while (n > 0 && off < file->size) {
		if (!(start = get_off_addr(file, off, 0)))
			goto out;

		// determine what the copy size should be
		size = BSIZE - off % BSIZE;  // rest of the block.
		if (file->size < off + size)
			size = file->size - off;  // rest of the file.
		if (nread + size > n)
			size = n - nread;  // rest of requested bytes.
		memcpy(buf + nread, start, size);
		n -= size;
		nread += size;
		off += size;
	}

out:
	return nread;
}

/**
 * Basic version of the POSIX read system call.
 * 
 * @param fd	File descriptor to read from
 * @param buf	Buffer wherein bytes read are stored
 * @param count	Number of bytes requested
 * @return		Number of bytes actually read
 */
unsigned int fs_read(int fd, void *buf, unsigned int count)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (!buf)
		return -1;	

	struct hfs_inode *file = dentry_get_inode(openfiles[fd].f_dentry);
	if (file->type != T_REG)
		return -EINVTYPE;

	int ret;
	unsigned int off = openfiles[fd].offset;
	ret = do_read(file, off, buf, count);
	if (ret < 0)
		goto bad_read;
	openfiles[fd].offset += ret;

bad_read:
	return ret;
}

/**
 * Write to a file.
 * 
 * @param file	The file's inode
 * @param off	The starting offset
 * @param buf	Buffer wherein bytes to be written are stored
 * @param n		Number of bytes left to be written
 * @return		The number of bytes written
 */
unsigned int do_write(struct hfs_inode *file, unsigned int *off, 
					  void *buf, unsigned int n)
{
	int nwritten = 0;
	int size;	// size of each write
	char *start;

	inode_touch_mtime(file);

	while (n > 0) {
		if (!(start = get_off_addr(file, *off, 1)))
			return -EALLOC;

		// determine what the copy size should be
		size = n;
		if (size > BSIZE - *off % BSIZE)
			size = BSIZE - *off % BSIZE;
		memcpy(start, buf + nwritten, size);
		n -= size;
		nwritten += size;
		*off += size;
	}

	return nwritten;
}

/**
 * Basic version of the POSIX write system call.
 * 
 * @param fd	File descriptor to write to
 * @param buf	Buffer wherein bytes to write are stored
 * @param count	Number of bytes requested
 * @return		Number of bytes actually written
 */
unsigned int fs_write(int fd, void *buf, unsigned int count)
{
	if (!fd_inuse(fd))
		return -EINVFD;
	if (!buf)
		return -1;	

	struct hfs_inode *file = dentry_get_inode(openfiles[fd].f_dentry);
	if (file->type != T_REG)
		return -EINVTYPE;

	int ret;
	unsigned int off = openfiles[fd].offset;
	ret = do_write(file, &off, buf, count);
	
	if (ret) {
		openfiles[fd].offset = off;
		if (off > file->size)
			file->size = off;
	}
	return ret;
}

/**
 * Basic version of the POSIX unlink system call.
 */
int fs_unlink(const char *pathname)
{
	struct hfs_inode *dir;
	struct hfs_dentry *dent;
	if (!(dent = dir_lookup(pathname, &dir)) || !dir)
		return -ENOFOUND;
	if (dentry_get_inode(dent)->type == T_DIR)
		return -EINVTYPE;
	unlink_dent(dent);
	inode_touch_mtime(dir);
	return 0;
}

/**
 * Basic version of the POSIX link system call.
 */
int fs_link(const char *oldpath, const char *newpath)
{
	struct hfs_dentry *olddent = lookup(oldpath);
	if (!olddent)
		return -ENOFOUND;
	struct hfs_inode *inode = dentry_get_inode(olddent);
	if (inode->type == T_DIR)
		return -EINVTYPE;

	// make sure newpath doesn't already exist
	struct hfs_inode *dir;
	if (dir_lookup(newpath, &dir)) {
		pr_warn("%s already exists.\n", newpath);
		return -EEXISTS;
	}
	if (!dir)
		return -ENOFOUND;

	char filename[DENTRYNAMELEN + 1];
	get_filename(newpath, filename);
	if (!new_dentry(dir, inode, filename)) {
		return -EALLOC;
	} else {
		return 0;
	}
}

/**
 * Basic version of the POSIX mkdir() system call.
 */
int fs_mkdir(const char *pathname)
{
	struct hfs_inode *dir;

	if (dir_lookup(pathname, &dir)) {
		pr_warn("%s already exists.\n", pathname);
		return -EEXISTS;
	}
	if (!dir)
		return -ENOFOUND;
	if (dir->type != T_DIR)
		return -EINVTYPE;

	char filename[DENTRYNAMELEN + 1];
	get_filename(pathname, filename);

	if (strlen(filename) > DENTRYNAMELEN)
		return -EINVNAME;

	if (!(do_creat(dir, filename, T_DIR)))
		return -EALLOC;

	return 0;
}

/**
 * Check if a block of dentries contains no valid dentries
 * (apart from the . and .. entries).
 */
static int dir_block_isempty(uint32_t b)
{
	char *block = BLKADDR(b);
	struct hfs_dentry *dent;
	
	for_each_block_dent(dent, block) {
		if (dent->reclen == 0)
			break;
		if (dent->inum) {
			if (strcmp(dentry_get_name(dent), ".") == 0
					|| strcmp(dentry_get_name(dent), "..") == 0)
				continue;
			return -1;
		}
	}
	return 0;
}

/**
 * Check if a directory is empty.
 */
static int dir_isempty(struct hfs_inode *dir)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		struct hfs_dentry *dent;
		for_each_inline_dent(dent, dir) {
			if (dent->reclen == 0)
				break;
			if (dent->inum)
				return -1;
		}
		return 0;
	} 
#endif  // _HFS_INLINE_DIRECTORY

	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			if (!dir_block_isempty(dir->data.blocks[i]))
				return -ENOTEMPTY;
		}
	}
	return 0;
}

/**
 * Basic version of the POSIX rmdir() system call.
 */
int fs_rmdir(const char *pathname)
{
	struct hfs_inode *parent;
	struct hfs_dentry *dent = dir_lookup(pathname, &parent);
	if (!dent || !parent)
		return -ENOFOUND;
	if (dentry_get_inode(dent)->type != T_DIR)
		return -EINVTYPE;
	if (!dir_isempty(dentry_get_inode(dent)))
		return -ENOTEMPTY;

	unlink_dent(dent);
	parent->nlink--;
	inode_touch_mtime(parent);
	return 0;
}

/**
 * Set the target of a symbolic link (i.e. write the path name).
 * We always treat the symlink inode as a brand-new inode because of
 * the assumption that a symlink CANNOT be updated.
 */
static int symlink_set_target(struct hfs_inode *symlink, const char *target)
{
	memset(symlink->data.symlink_path, 0, INODE_BLOCKS_SIZE);

	/* Attempt inline link */
	int linklen = strlen(target) + 1;
	if (linklen <= INODE_BLOCKS_SIZE) {
		strncpy(symlink->data.symlink_path, target, linklen);
		symlink->flags |= I_INLINE;
		return 0;
	}

	/* Regular symlink, stored in a block */
	if (!(symlink->data.blocks[0] = alloc_data_block()))
		return -EALLOC;
	char *path = BLKADDR(symlink->data.blocks[0]);
	strncpy(path, target, linklen);
	return 0;
}

/**
 * Basic version of the POSIX symlink() system call.
 * 
 * fs_symlink() creates a symbolic link named linkpath which
 * contains the string target.
 * 
 * Note that a symbolic link CANNOT be edited; it can only be replaced
 * by a new symbolic link. So when setting the path in the symlink,
 * we can safely assume that there will be no pre-existing data.
 */
int fs_symlink(const char *target, const char *linkpath)
{
	int ret;
	struct hfs_dentry *dent;
	struct hfs_inode *dir = NULL, *symlink;

	if (strlen(target) > BSIZE)
		return -EINVAL;

	if ((do_lookup(linkpath, &dir)))
		return -EEXISTS;
	if (!dir)
		return -ENOFOUND;
	
	symlink = alloc_inode(T_SYM);
	if (!symlink)
		return -EALLOC;

	char filename[DENTRYNAMELEN];
	get_filename(linkpath, filename);
	if (strlen(filename) > DENTRYNAMELEN)
		return -EINVNAME;

	if (!(dent = do_creat(dir, filename, T_SYM)))
		return -EALLOC;

	symlink = inode_from_inum(dent->inum);
	if ((ret = symlink_set_target(symlink, target)) < 0)
		unlink_dent(dent);

	return ret;
}

/**
 * Read the path stored in a symbolic link.
 * @param pathname	the path to the symbolic link in question.
 * @param buf		user-provided buffer wherein to store the result.
 * @param bufsize	size of the user-provided buffer.
 * @return			the number of bytes stored in buf.
 */
int fs_readlink(const char *pathname, char *buf, size_t bufsize)
{
	struct hfs_dentry *dent;
	struct hfs_inode *symlink;
	char *link;
	int nbytes = 0, linklen;

	if (!buf || bufsize == 0)
		return -EINVAL;
	if (!(dent = lookup(pathname)))
		return -ENOFOUND;
	
	symlink = inode_from_inum(dent->inum);
	if (symlink->type != T_SYM)
		return -EINVTYPE;

	/* Determine where link is stored (inline vs block) */
	link = (symlink->flags & I_INLINE) ?
			(char *)symlink->data.symlink_path : 
			BLKADDR(symlink->data.blocks[0]);
	linklen = strlen(link);

	/* If link is longer than buffer size, truncate copy size */
	nbytes = (linklen < bufsize) ? linklen : bufsize;
	strncpy(buf, link, nbytes);

	return nbytes;
}

/**
 * Perform stat on a given inode.
 */
static void do_stat(uint32_t inum, struct hfs_stat *statbuf)
{
	memset(statbuf, 0, sizeof(struct hfs_stat));

	struct hfs_inode *inode = inode_from_inum(inum);
	statbuf->st_ino = inum;
	statbuf->st_nlink = inode->nlink;
	statbuf->st_size = inode->size;
	statbuf->st_type = inode->type;
	statbuf->st_accesstime = inode->atime;
	statbuf->st_modifytime = inode->mtime;
	statbuf->st_changetime = inode->ctime;
	for (int i = 0; i < NBLOCKS; i++) {
		if (inode->data.blocks[i])
			statbuf->st_blocks++;
	} 
}

/**
 * Basic implementation of the POSIX stat() system call.
 */
int fs_stat(const char *pathname, struct hfs_stat *statbuf)
{
	struct hfs_dentry *dent;

	if (!statbuf)
		return -EINVAL;
	if (!(dent = lookup(pathname)))
		return -ENOFOUND;

	do_stat(dent->inum, statbuf);
	return 0;
}

/**
 * Prints a list of enabled features.
 */
void print_features(void)
{
	pr_info(KBLD "\nENABLED FEATURES: \n" KNRM);

#ifdef _HFS_INLINE_DIRECTORY
	pr_info(KBLD KGRN "[ON]  " KNRM);
#else
	pr_info(KBLD KYEL "[OFF] " KNRM);
#endif
	pr_info("Inline directories\n");

#ifdef _HFS_DIRHASH
	pr_info(KBLD KGRN "[ON]  " KNRM);
#else
	pr_info(KBLD KYEL "[OFF] " KNRM);
#endif
	pr_info("Dirhash\n");

	pr_info("\n");
}

/**
 * Change working directory.
 */
int fs_chdir(const char *pathname)
{
	struct hfs_dentry *dent;
	struct hfs_inode *dir;

	if (!(dent = lookup(pathname)))
		return -ENOFOUND;
	dir = dentry_get_inode(dent); 
	if (dir->type != T_DIR)
		return -EINVTYPE;
	
	cwd = dent;
	return 0;
}

/*
 * Allocates space for file system in memory.
 */
int fs_mount(unsigned long size)
{
	size_t fs_size;
	int fs_is_new = 0;
	int fd;

	if (fs) {
		printf("Error: file system is already mounted.\n");
		return -1;
	}

	if (size > MAXFSSIZE) {
		printf("Error: file system size cannot be greater than 1GB.\n");
		return -1;
	}

	if (access("fs.img", F_OK) != -1) {
		fd = open("fs.img", O_RDWR);
	} else {
		fd = open("fs.img", O_RDWR | O_CREAT, 0666);
		fs_is_new = 1;
	}
	if (!fd) {
		perror("open");
		return -1;
	}

	// If using an existing file system image, mmap the entire file.
	// Otherwise, create a file system of the desired size.
	if (!fs_is_new) {
		struct stat statbuf;
		if (fstat(fd, &statbuf) < 0) {
			perror("stat");
			return -1;
		}
		fs_size = statbuf.st_size;
	} else {
		printf("Creating new file system...\n");
		if (ftruncate(fd, 0) < 0) {
			perror("ftruncate");
			return -1;
		}
		lseek(fd, size, SEEK_SET);
		if (write(fd, "\0", 1) < 0) {
			perror("Reset image: write");
			return -1;
		}
		fs_size = size;
	}

	fs = mmap(NULL, fs_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!fs) {
		perror("mmap");
		return -1;
	}

	// Initialize in-memory caches
	if (init_caches() < 0)
		return -1;

	// If file system is newly created, initialise everything.
	if (fs_is_new) {
		if (init_fs(fs_size) < 0)
			return -1;
	}

	init_fd();
	read_sb();
	close(fd); 

	sb->last_mounted = time(NULL);

	cwd = &sb->rootdir;

	pr_info("File system successfully mounted.\n");
	print_features();
	return 0;
}

/**
 * Unmounts the file system.
 */
int fs_unmount(void)
{
	if (!fs)
		return -1;

	free_caches();
	printf("Quitting fsemu...\n");
	fflush(stdout);
	munmap(fs, sb->size * BSIZE);
	fs = NULL;
	return 0;
}

/**
 * Completely reset the file system.
 */
int fs_reset(void)
{
	if (!fs)
		return -1;

	unsigned long fs_size = sb->size * BSIZE;
	memset(fs, 0x0, fs_size);
	if (init_fs(fs_size) < 0)
		return -1;
	if (init_caches() < 0)
		return -1;
	init_fd();
	read_sb();
	return 0;
}
