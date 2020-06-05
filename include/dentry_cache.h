/**
 * fsemu/include/dentry_cache.h
 * 
 * In-memory hash table for directory entries.
 */

#ifndef __DENTRY_CACHE_H__
#define __DENTRY_CACHE_H__

#include "fs.h"

#define TABLESIZE	32
#define PRIMESIZE	31

struct hashdata {
	struct dentry *dent;
};

struct hashtable {
	struct hashtable *prev;
	struct hashtable *next;
	struct hashdata data[TABLESIZE];
};

/**
 * Keep track of the head and tail of the linked list of
 * hashtables. Used in LRU replacement policy.
 */
struct dentry_cache {
	struct hashtable *head;
	struct hashtable *tail;
};

int dentry_cache_init_all(void);
void dentry_cache_free_all(void);
struct dentry *dentry_cache_lookup(struct inode *dir, const char *name);
int dentry_cache_insert(struct inode *dir, struct dentry *dent);
int dentry_cache_remove(struct inode *dir, const char *name);

void db_print_dentry_cache(void);

#endif  // __DENTRY_CACHE_H__