#include "fsemu.h"
#include "fs.h"
#include "util.h"

#include <stdio.h>

static char *type_names[] = {
	[T_UNUSED]  "UNUSED",
	[T_REG]     "FILE  ",
	[T_DIR]     "DIR   ",
	[T_DEV]     "DEVICE"
};

/**
 * Print out a block of dentries in something resembling 
 * the following format:
 * (Does not print out unused dentries)
 * 
 * DIR      .        512   [0x7fd81340] [2]
 * DIR      ..       512   [0x7fd412a0] [4]
 * FILE     hello.c  54    [0x7f4d9fd0] [1]
 * FILE     hello    231   [0x7f4e0200] [1]
 */
static inline void print_dirents(uint32_t block_num)
{
	struct dentry *dents = (struct dentry *)BLKADDR(block_num);
	for (int i = 0; i < DENTPERBLK; i++) {
		if (dents[i].inode) {
			printf("%s %-16s %-6d [%p] [%d]\n", 
				type_names[dents[i].inode->type],
				dents[i].name, dents[i].inode->size,
				dents[i].inode, dents[i].inode->nlink);
		}
	}
}

/*
 * Perform ls on a given directory
 */
static void ls_dir(struct dentry *dent)
{
	printf("%s \n", dent->name);
	struct inode *inode = dent->inode;
	if (inode->type != T_DIR)
		return;
	for (int i = 0; i < NADDR; i++) {
		if (inode->data[i]) {
			print_dirents(inode->data[i]);
		}
	}
}

int ls(void)
{
	// For now only prints out the root's contents
	ls_dir(&sb->rootdir);
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
			printf(" [%d] %s (off=%lx)\n", i, openfiles[i].f_dentry->name,
											openfiles[i].offset);
		}
	}
	puts("");
}