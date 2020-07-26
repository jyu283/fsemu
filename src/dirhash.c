/**
 * fsemu/src/dirhash.c
 * 
 * An in-memory LRU directory entry cache for O(1) lookup.
 */

#include "fsemu.h"
#include "dirhash.h"

#include <stdlib.h>

struct hfs_dirhash *dirhash;

static inline struct hfs_dirhash_header *hfs_dirhash_get_table(int id)
{
    return (struct hfs_dirhash_header *)&dirhash->tables[id];
}

static inline int hfs_dirhash_get_id(struct hfs_dirhash_header *dt)
{
    return ((struct hfs_dirhash_table *)dt - dirhash->tables);
}

/**
 * Form a doubly-linked list from all the dirhash tables.
 */
static void init_dirhash_tables(void)
{
    dirhash->head = hfs_dirhash_get_table(0);
    dirhash->tail = hfs_dirhash_get_table(0);

    dirhash->head->prev = NULL;
    dirhash->tail->next = NULL;

    struct hfs_dirhash_header *curr, *next;
    for (int i = 0; i < HFS_DIRHASH_SIZE - 1; i++) {
        curr = (struct hfs_dirhash_header *)hfs_dirhash_get_table(i);
        next = (struct hfs_dirhash_header *)hfs_dirhash_get_table(i + 1);
        curr->next = next;
        next->prev = curr;
    }
#ifdef HFS_DEBUG
    hfs_dirhash_dump();
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
    printf("(seq=%ld): ", dt->header.seqno);
    if (dt->header.seqno == 0) {
        printf("EMPTY");
        return;
    }

    for (int i = 0; i < HFS_DIRHASH_TABLESIZE; i++) {
        if (dt->data[i].seqno != dt->header.seqno)
            continue;
        if (dt->data[i].dent)
            printf("[%ld|%.16s] ", dt->data[i].seqno, dt->data[i].dent->name);
        else
            printf("[%ld|----] ", dt->data[i].seqno);
    }
}

/**
 * Debug function: Print out all the contents of dirhash.
 */
void hfs_dirhash_dump(void)
{
    if (!dirhash)
        return;

    struct hfs_dirhash_header *dh;
    for (dh = dirhash->head; dh; dh = dh->next) {
        printf("DIRHASH #%d ", hfs_dirhash_get_id(dh));
        dirhash_table_dump((struct hfs_dirhash_table *)dh);
        puts("");
    }
}
