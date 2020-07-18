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
	[T_DEV]     = "DEVICE"
};

static inline void print_inode(struct hfs_inode *inode, const char *name)
{
	printf("%s %-3d %-16s %-6d inum=%d",
		type_names[inode->type], inode->nlink, name,
		inode->size, inum(inode));
}

static inline void print_dentry(struct hfs_dentry *dent)
{
	struct hfs_inode *inode = dentry_get_inode(dent);
	print_inode(inode, dentry_get_name(dent));
	printf("  rlen=%-3d nlen=%-3d off=%lu\n", dent->reclen, 
					dent->namelen, (unsigned long)dent % BSIZE);
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
			print_dentry(dent);
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

	// if inline directory
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
				print_dentry(inline_dent);
			}
		}
	} else {
		for (int i = 0; i < NBLOCKS; i++) {
			if (dir->data.blocks[i]) {
				print_dentry_block(dir->data.blocks[i]);
			}
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