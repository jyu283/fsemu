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
    uint64_t            seqno;
    uint64_t            name_hash;
    struct hfs_dentry   *dent;
    struct hfs_dirhash_entry    *next_dirhash;
};

struct hfs_dirhash_header {
    struct hfs_dirhash_header   *prev;
    struct hfs_dirhash_header   *next;
    uint64_t                    seqno;
};

struct hfs_dirhash_table {
    struct hfs_dirhash_header   header;
    struct hfs_dirhash_entry    data[HFS_DIRHASH_TABLESIZE];
};

struct hfs_dirhash {
    struct hfs_dirhash_header   *head;
    struct hfs_dirhash_header   *tail;
    struct hfs_dirhash_table    tables[HFS_DIRHASH_SIZE];
};

extern struct hfs_dirhash *dirhash;

int hfs_dirhash_init(void);

#endif  // __DIRHASH_H__