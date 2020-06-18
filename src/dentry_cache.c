/**
 * fesmu/src/dentry_cache.c
 * 
 * In-memory hash table for directory entries.
 */

#include "fsemu.h"
#include "dentry_cache.h"
#include "fserror.h"

#include <stdlib.h>

#define HASH_PVALUE	((int)31)
#define HASH_MVALUE	((int)(1e9 + 9))

static struct hashtable *hashtables;
static struct dentry_cache dcache;

static void db_print_hashtable(struct hashtable *ht)
{
	struct dentry *dent;

	printf("[");
	for (int i = 0; i < PRIMESIZE; i++) {
		dent = ht->data[i].dent;
		if (dent)
			printf("(%p, %s) ", dent, dentry_get_name(dent));
	}
	printf("]\n");
}

/**
 * Debug function:
 * Print out the entire dentry cache.
 */
void db_print_dentry_cache(void)
{
	struct hashtable *ht;

	ht = dcache.head;
	while (ht) {
		if (ht == dcache.head)
			printf("HEAD >> ");
		else if (ht == dcache.tail)
			printf("TAIL >> ");
		else
			printf("        ");
		db_print_hashtable(ht);
		ht = ht->next;
	}
}

/**
 * Initialise dentry cache. Link all hashtables.
 */
int dentry_cache_init_all(void)
{
	hashtables = malloc(sizeof(struct hashtable) * DENTCACHESIZE);
	if (!hashtables)
		return -EALLOC;
	memset(hashtables, 0, sizeof(*hashtables));

	// Link all the hashtables.
	struct hashtable *prev = NULL;
	struct hashtable *curr;
	for (int i = 0; i < DENTCACHESIZE; i++) {
		curr = &hashtables[i];
		curr->prev = prev;
		prev = curr;
		if (i == DENTCACHESIZE - 1) {
			curr->next = NULL;
		} else {
			curr->next = &hashtables[i + 1];
		}
	}

	dcache.head = &hashtables[0];
	dcache.tail = &hashtables[0];
	return 0;
}

/**
 * Free dentry cache.
 */
void dentry_cache_free_all(void)
{
	free(hashtables);
	dcache.head = NULL;
	dcache.tail = NULL;
	hashtables = NULL;
}

/**
 * Hash a filename.
 */
static inline int hash(const char *s) {
    long hash_value, p_pow; 
    register int i;

    hash_value= 0;
    p_pow = 1;
    for (i = 0; i < strlen(s); i++) {
        hash_value = (hash_value + (s[i] - 'a' + 1) * p_pow) % HASH_MVALUE;
        p_pow = (p_pow * HASH_PVALUE) % HASH_MVALUE;
    }
    return abs(hash_value % PRIMESIZE);  
}
