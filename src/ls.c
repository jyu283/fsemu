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

static inline void print_inode(struct inode *inode, const char *name)
{
	printf("%s %-3d %-16s %-6d inum=%d",
		type_names[inode->type], inode->nlink, name,
		inode->size, inum(inode));
}

/**
 * Print out a block of dentries in something resembling 
 * the following format:
 * (Does not print out unused dentries)
 */
static inline void print_dirents(uint32_t block_num)
{
	struct dentry_block *block = (struct dentry_block *)BLKADDR(block_num);
	struct inode *inode;
	for (int i = 0; i < DENTPERBLK; i++) {
		if (block->dents[i].inum) {
			inode = dentry_get_inode(&block->dents[i]);
			print_inode(inode, dentry_get_name(&block->dents[i]));
			puts("");
		}
	}
}

/*
 * Perform ls on a given directory
 */
static void ls_dir(struct inode *dir, const char *name)
{
	printf("%s \n", name);
	if (dir->type != T_DIR)
		return;

	// if inline directory
	if (inode_is_inline_dir(dir)) {
		printf("[Inline directory]\n");
		struct dentry_inline *inline_dent;
		print_inode(dir, ".");
		puts("");
		print_inode(&inodes[dir->data.inline_dir.p_inum], "..");
		puts("");
		for_each_inline_dent(inline_dent, dir) {
			if (!inline_dent->reclen)
				break;
			if (inline_dent->inum) {
				print_inode(&inodes[inline_dent->inum], inline_dent->name);
				printf("  reclen=%d namelen=%d\n", 
						inline_dent->reclen, inline_dent->namelen);
			}
		}
		return;
	}

	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			print_dirents(dir->data.blocks[i]);
		}
	}
}

int ls(const char *pathname)
{
	struct inode *src;
	char *name = "/";
	if (strcmp(pathname, "/") == 0) {
		src = get_root_inode();
	} else {
		struct dentry *src_dent;
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