/**
 * fsemu/src/dirhash.c
 * 
 * An in-memory LRU directory entry cache for O(1) lookup.
 */

#include "fsemu.h"
#include "dirhash.h"
#include "fserror.h"
#include "util.h"

#include <stdlib.h>

struct hfs_dirhash *dirhash;

static inline struct hfs_dirhash_table *hfs_dirhash_get_table(int id)
{
    return &dirhash->tables[id];
}

static inline int hfs_dirhash_get_id(struct hfs_dirhash_table *dt)
{
    return (dt - dirhash->tables);
}

/**
 * "Touch" an LRU entry and promoting it to be the head.
 */
static inline void lru_touch(struct hfs_dirhash_table *dt)
{
    struct hfs_dirhash_table *prev = dt->prev;
    struct hfs_dirhash_table *next = dt->next;

    // hdr is already most recently used.
    if (!prev)
        return;

    prev->next = next;
    if (!next) {
        dirhash->tail = prev;
    } else {
        next->prev = prev;
    }

    // Move to top of list
    dt->prev = NULL;
    dt->next = dirhash->head;
    dt->next->prev = dt;
    dirhash->head = dt;
} 

/**
 * Return the least recently used table, but since it is 
 * used right now, we promote it to the head and consequently
 * return the new head of the LRU.
 */
static inline struct hfs_dirhash_table *lru_get_last(void)
{
    lru_touch(dirhash->tail);
    return dirhash->head;
}

/**
 * Verify that a dirhash table is still valid for a directory.
 * i.e. 1) The sequence numbers match
 *      2) The table and inode are marked for each other.
 */
static inline int dirhash_table_is_valid(struct hfs_dirhash_table *dt,
                                         struct hfs_inode *dir)
{
    return (dt->seqno == dir->data.dirhash_rec.seqno 
            && dt->inum == inum(dir));
}

/**
 * This function is used to hash a file name to determine its location
 * in a dirhash table.
 * 
 * Courtesy of the Gods on StackOverflow:
 * https://stackoverflow.com/questions/11413860/best-string-hashing-function
 * -for-short-filenames
 */
static inline int fnv_hash(const char *name, int namelen)
{
    const unsigned char *p = (const unsigned char *)name;
    unsigned int h = 2166136261;

    for (int i = 0; i < namelen; i++)
        h = abs((h*16777619) ^ p[i]);

    return (h % HFS_DIRHASH_TABLESIZE);
}

/**
 * A separate hash function to verify a file name. The hope is that with the
 * combination of these two functions it would be virtually impossible to 
 * misidentify a file even if we don't store/check the full name.
 */
static uint32_t name_hash(const char *name, int namelen)
{
    uint32_t hash = 5381;
    const unsigned char *str = (const unsigned char *)name;

    for (int i = 0; i < namelen; i++)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    return hash;
}

/**
 * Add an entry to the given dirhash table.
 * 
 * If there is a collision, use linear probing.
 * 
 * This function assumes that the table is at a reasonable capacity.
 * When the directory grows beyond a certain size, we should swtich to a
 * different indexing mechanism.
 * 
 * TODO: Need to think about the case where two different file names
 * are hashed to the same index and also have the same name_hash value.
 * Unlikely, very unlikely, but should be considered.
 */
static void put_dentry(struct hfs_dirhash_table *dt, struct hfs_dentry *dent)
{
    int h = fnv_hash(dent->name, dent->namelen);
    uint64_t h2 = name_hash(dent->name, dent->namelen);
    struct hfs_dirhash_entry *ent = &dt->data[h];

    // If at index h there is already an entry belonging to this directory,
    // we use linear probing to find an available spot
    while (ent->seqno == dt->seqno) {
        if (ent->name_hash == h2)
            break;
        // wrap around if h reaches the end of the table 
        h = (h < HFS_DIRHASH_TABLESIZE - 1) ? (h + 1) : 0;
        ent = &dt->data[h];
    }

    // Fill in the entry
    ent->seqno = dt->seqno;
    ent->name_hash = h2;
    ent->dent = dent;
    ent->next_dirhash = NULL;

    lru_touch(dt);
}

/**
 * Place all the entries in a directory into the specified dirhash table.
 */
static void put_directory(struct hfs_dirhash_table *dt, struct hfs_inode *dir)
{
    char *block = BLKADDR(dir->data.dirhash_rec.block);
    struct hfs_dentry *dent;
    for_each_block_dent(dent, block) {
        if (dent->reclen == 0)
            break;
        if (dent->inum == 0)
            continue;
        put_dentry(dt, dent);
    }
}

/**
 * Allocate a dirhash table for the specified directory,
 * and load all directory entries.
 */
static struct hfs_dirhash_table *dir_alloc_table(struct hfs_inode *dir)
{
    struct hfs_dirhash_table *dt;
    dt = lru_get_last();
    dt->seqno++;
    dt->inum = inum(dir);
    dir->data.dirhash_rec.seqno = dt->seqno;
    dir->data.dirhash_rec.id = hfs_dirhash_get_id(dt);

    put_directory(dt, dir);
    return dt;
}

/**
 * Cache a directory in dirhash.
 * 
 * Assume that currently the directory is NOT cached.
 */
int hfs_dirhash_put_dir(struct hfs_inode *dir)
{
    struct hfs_dirhash_table *dt = dir_alloc_table(dir);
    return 0;
}

/**
 * Add a directory entry to the dirhash table belonging to 
 * the specified directory.
 */
int hfs_dirhash_put(struct hfs_inode *dir, struct hfs_dentry *dent)
{
    int id;
    int dir_inum = inum(dir);

    struct hfs_dirhash_table *dt = NULL;

    // If inode indicates that it has a table, check if it's still valid.
    if ((id = dir->data.dirhash_rec.id)) {
        dt = hfs_dirhash_get_table(id);
        if (!dirhash_table_is_valid(dt, dir))
            dt = NULL;
    }

    if (!dt) {
        // NOTE: dir_alloc_table will have put dent into cache
        dt = dir_alloc_table(dir);
    } else {
        put_dentry(dt, dent);
    }
    return 0;
}

/**
 * Form a doubly-linked list from all the dirhash tables.
 */
static void init_dirhash_tables(void)
{
    dirhash->head = hfs_dirhash_get_table(0);
    dirhash->tail = hfs_dirhash_get_table(HFS_DIRHASH_SIZE - 1);

    dirhash->head->prev = NULL;
    dirhash->tail->next = NULL;

    struct hfs_dirhash_table *curr, *next;
    for (int i = 0; i < HFS_DIRHASH_SIZE - 1; i++) {
        curr = hfs_dirhash_get_table(i);
        next = hfs_dirhash_get_table(i + 1);
        curr->next = next;
        next->prev = curr;
    }
#ifdef HFS_DEBUG
    // hfs_dirhash_dump();
#endif
}

/**
 * Allocate and initialize dirhash.
 */
int hfs_dirhash_init(void)
{
    dirhash = malloc(sizeof(struct hfs_dirhash));
    if (!dirhash) {
        pr_warn("Failed to initialize dirhash.\n");
        return -1;
    }
    memset(dirhash, 0, sizeof(struct hfs_dirhash));
    init_dirhash_tables();    

    pr_info("Dirhash initialized successfully (%ld bytes)\n", 
                                    sizeof(struct hfs_dirhash));
    return 0;
}

/**
 * Free dirhash tables.
 */
void hfs_dirhash_free(void)
{
    free(dirhash);
    dirhash = NULL;
}

/**
 * Print out a single dirhash table.
 */
static void dirhash_table_dump(struct hfs_dirhash_table *dt)
{
    printf("(seq=%d): ", dt->seqno);
    if (dt->seqno == 0) {
        printf("EMPTY");
        return;
    }

    for (int i = 0; i < HFS_DIRHASH_TABLESIZE; i++) {
        if (dt->data[i].seqno != dt->seqno)
            continue;
        if (dt->data[i].dent)
            printf("[%d|%.16s] ", dt->data[i].seqno, dt->data[i].dent->name);
        else
            printf("[%d|----] ", dt->data[i].seqno);
    }
}

/**
 * Debug function: Print out all the contents of dirhash.
 */
void hfs_dirhash_dump(void)
{
    if (!dirhash)
        return;

    struct hfs_dirhash_table *dt;
    for (dt = dirhash->head; dt; dt = dt->next) {
        printf("DIRHASH #%d ", hfs_dirhash_get_id(dt));
        dirhash_table_dump((struct hfs_dirhash_table *)dt);
        puts("");
    }
}
