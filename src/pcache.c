/**
 * fsemu/include/pcache.c
 * 
 * A global hashtable for looking up full pathnames.
 */

#include "pcache.h"
#include "fsemu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pcache *pcache;

#define HASH_PVALUE	((int)31)
#define HASH_MVALUE	((int)(1e9 + 9))

/**
 * Primary hash function.
 * Hash a filename to an index in the hashtable (pcache).
 */
static int hash(const char *s) {
    long hash_value, p_pow; 
    register int i;

    hash_value= 0;
    p_pow = 1;
    for (i = 0; i < strlen(s); i++) {
        hash_value = (hash_value + (s[i] - 'a' + 1) * p_pow) % HASH_MVALUE;
        p_pow = (p_pow * HASH_PVALUE) % HASH_MVALUE;
    }
    return abs(hash_value % PATH_CACHE_SIZE);  
}

/**
 * A separate hash function to compare pathnames. 
 * path_cache_entry.hash2 uses this function.
 */
static unsigned long hash2(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c

    return hash;
}

/**
 * Initialise the pathname cache.
 */
int pcache_init(void)
{
	pcache = malloc(sizeof(*pcache));
	if (!pcache) {
		printf("Error: failed to create pathname cache.\n");
		return -1;
	}
	memset(pcache, 0, sizeof(*pcache));
	return 0;
}

/**
 * Free the pathname cache.
 */
void pcache_free(void)
{
	free(pcache);
	pcache = NULL;
}

/**
 * pcache_lookup()
 * 
 * Looks up a path in the hash table. Check the hash2 value at the 
 * calculated index to see if the path represented by the entry is
 * the same as the one requested.
 */
struct dentry *pcache_lookup(const char *pathname, struct inode **pi)
{
	struct dentry *dent = NULL;
	int h = hash(pathname);
	int h2 = hash2(pathname);

	if (pcache->data[h].hash2 == h2)
		dent = pcache->data[h].dent;
	
	if (pi)
		*pi = pcache->data[h].pi;

#ifdef DEBUG
	if (dent)
		pr_debug("PCACHE HIT:  %s.\n", pathname);
	else 
		pr_debug("PCACHE MISS: %s.\n", pathname);
#endif

	return dent;
}

/**
 * pcache_put()
 *
 * Creates an entry for the given pathname.
 */
void pcache_put(const char *pathname, struct dentry *dent, struct inode *pi)
{
	int h = hash(pathname);
	int h2 = hash2(pathname);

	pcache->data[h].dent = dent;
	pcache->data[h].hash2 = h2;
	pcache->data[h].pi = pi;
}

void db_pcache_dump(void)
{
	printf("Path cache dump: \n");
	for (int i = 0; i < PATH_CACHE_SIZE; i++) {
		if (pcache->data[i].dent) {
			printf("[%d] %s inum #%d (h2=%ld, addr=%p)\n", i, 
					pcache->data[i].dent->name,
					pcache->data[i].dent->inum,
					pcache->data[i].hash2, pcache->data[i].dent);
		}
	}
}
