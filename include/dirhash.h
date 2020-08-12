/**
 * fsemu/include/dirhash.h
 * 
 * An in-memory LRU directory entry cache for O(1) lookup. 
 * A more aggressive implementation of UFS dirhash.
 */

#ifndef __DIRHASH_H__
#define __DIRHASH_H__

#include "fsemu.h"
#include "fs.h"

/* Total number of hash tables */
#define HFS_DIRHASH_SIZE    100

/* Size of each hash table, a prime number. */
#define HFS_DIRHASH_TABLESIZE   97

struct hfs_dirhash_entry {
    uint32_t            seqno;
    uint32_t            name_hash;
    struct hfs_dentry   *dent;
    struct hfs_dirhash_entry    *next_dirhash;
};

struct hfs_dirhash_table {
    uint32_t                    seqno;
    uint32_t                    inum;
    struct hfs_dirhash_table   *prev;
    struct hfs_dirhash_table   *next;
    struct hfs_dirhash_entry    data[HFS_DIRHASH_TABLESIZE];
};

struct hfs_dirhash {
    struct hfs_dirhash_table   *head;
    struct hfs_dirhash_table   *tail;
    struct hfs_dirhash_table    tables[HFS_DIRHASH_SIZE];
};

extern struct hfs_dirhash *dirhash;

int hfs_dirhash_init(void);
void hfs_dirhash_free(void);
int hfs_dirhash_put(struct hfs_inode *dir, struct hfs_dentry *dent);
int hfs_dirhash_put_dir(struct hfs_inode *dir);

struct hfs_dirhash_entry *hfs_dirhash_lookup(struct hfs_inode *dir, 
                                             const char *name);

void hfs_dirhash_delete(struct hfs_inode *dir, const char *name);

static inline int inode_dirhash_enabled(struct hfs_inode *dir)
{
    return (dir->flags & I_DIRHASH);
}

#endif  // __DIRHASH_H__