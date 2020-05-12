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

/*
 * Print out a block of dentries in the following format:
 * (Does not print out unused dentries)
 * 
 * DIR      .           (0x7fd81340)
 * DIR      ..          (0x7fd412a0)
 * FILE     hello.c     (0x7f4d9fd0)
 * FILE     hello       (0x7f4e0200)
 */
static inline void print_dirents(uint32_t block_num)
{
    struct dentry *dents = (struct dentry *)BLKADDR(block_num);
    for (int i = 0; i < DENTPERBLK; i++) {
        if (dents[i].inode) {
            printf("%s %-16s \t[%p]\n", 
                type_names[dents[i].inode->type],
                dents[i].name, dents[i].inode);
        }
    }
}

/*
 * Perform ls on a given directory
 */
static void ls_dir(struct dentry *dent)
{
    printf("%s: \n", dent->name);
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
    return 0;
}