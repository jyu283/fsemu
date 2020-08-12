/**
 * fsemu/src/ls.c
 * 
 * A basic ls program. Except it isn't really a "program" in the sense
 * of a binary executable. They are really just functions that are a
 * simulacrum of their real-life Linux counterparts. 
 */

#include "fsemu.h"
#include "fs.h"
#include "util.h"

#include <stdio.h>

static char *type_names[] = {
	[T_UNUSED]  = "UNUSED",
	[T_REG]     = "FILE  ",
	[T_DIR]     = "DIR   ",
	[T_DEV]     = "DEVICE",
	[T_SYM]		= "LINK  ",
};

static inline void print_inode(struct hfs_inode *inode, const char *name)
{
	printf("%s %-3d %-16s %-6d inum=%d  ",
		type_names[inode->type], inode->nlink, name,
		inode->size, inum(inode));
}

static inline void print_dentry(struct hfs_dentry *dent, char *off_start)
{
	struct hfs_inode *inode = dentry_get_inode(dent);
	unsigned long off = (char *)dent - off_start;

	print_inode(inode, dentry_get_name(dent));
	printf("rlen=%-3d nlen=%-3d off=%lu-%lu\n", dent->reclen, 
						dent->namelen, off, off + dent->reclen);
}

/**
 * Print out a block of dentries in something resembling 
 * the following format:
 * (Does not print out unused dentries)
 */
static inline void print_dentry_block(uint32_t block_num)
{
	char *block = BLKADDR(block_num);
	struct hfs_dentry *dent;
	struct hfs_inode *inode;

	for_each_block_dent(dent, block) {
		if (dent->reclen == 0)
			break;
		if (dent->inum)
			print_dentry(dent, block);
	}
}

/*
 * Perform ls on a given directory
 */
static void ls_dir(struct hfs_inode *dir, const char *name)
{
	printf("%s \n", name);
	if (dir->type != T_DIR)
		return;

#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		printf("[Inline directory]\n");
		struct hfs_dentry *inline_dent;
		print_inode(dir, ".");
		puts("");
		print_inode(&inodes[dir->data.inline_dir.p_inum], "..");
		puts("");
		for_each_inline_dent(inline_dent, dir) {
			if (!inline_dent->reclen)
				break;
			if (inline_dent->inum) {
				print_dentry(inline_dent, (char *)&dir->data.inline_dir);
			}
		}
		return;
	} 
#endif
	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			print_dentry_block(dir->data.blocks[i]);
		}
	}
	
}

int ls(const char *pathname)
{
	struct hfs_inode *src;
	char *name = "/";
	if (strcmp(pathname, "/") == 0) {
		src = get_root_inode();
	} else {
		struct hfs_dentry *src_dent;
		if (!(src_dent = lookup(pathname)))
			return -1;
		src = dentry_get_inode(src_dent);
		name = dentry_get_name(src_dent);
	}

	// For now only prints out the root's contents
	ls_dir(src, name);
	puts("");
	pr_info("Total inodes in use: %lu\n", sb->inode_used);
	pr_info("Directories: %lu total. Files: %lu total.\n", 
							sb->ndirectories, sb->nfiles);
	pr_info("Total inline inodes: %lu\n", sb->inline_inodes);
	return 0;
}

/**
 * List all open file descriptors.
 */
void lsfd(void)
{
	printf("open file descriptors: \n");
	for (int i = 0; i < MAXOPENFILES; i++) {
		if (openfiles[i].f_dentry) {
			printf(" [%d] %s (off=%d)\n", i, 
					dentry_get_name(openfiles[i].f_dentry),
					openfiles[i].offset);
		}
	}
	puts("");
}

#ifdef HFS_DEBUG
static int do_show_inline(struct hfs_inode *dir)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		ls_dir(dir, "#####");
		return 0;
	} else {
		struct hfs_inode *inode;
		struct hfs_dentry *dent;
		char *block;
		for (int i = 0; i < NBLOCKS; i++) {
			if (dir->data.blocks[i] == 0)
				continue;
			block = BLKADDR(dir->data.blocks[i]);
			for_each_block_dent(dent, block) {
				if (!dent->reclen)
					break;
				if (dent->inum == 0)
					continue;
				if (strcmp(dent->name, ".") == 0 || 
						strcmp(dent->name, "..") == 0)
					continue;
				inode = inode_from_inum(dent->inum);
				if (inode->type != T_DIR)
					continue;
				if (do_show_inline(inode) == 0)
					return 0;
			}	
		}
	}
#endif
	return -1;
}

/**
 * Debug function:
 * Show inline directories.
 */
void show_inline(void)
{
	do_show_inline(get_root_inode());
}

static int do_show_regular(struct hfs_inode *dir)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (!inode_is_inline_dir(dir)) {
		ls_dir(dir, "#####");
		return 0;
	} else {
		struct hfs_inode *inode;
		struct hfs_dentry *dent;
		for_each_inline_dent(dent, dir) {
			if (!dent->reclen)
				break;
			if (dent->inum == 0)
				continue;
			inode = inode_from_inum(dent->inum); 
			if (inode->type != T_DIR)
				continue;
			if (do_show_regular(inode) == 0)
				return 0;
		}
	}
#endif
	return -1;
}

/**
 * Debug function:
 * Show regular directories.
 */
void show_regular(void)
{
	do_show_regular(get_root_inode());
}

#endif  // HFS_DEBUG
