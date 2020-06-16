/**
 * fsemu/include/pcache.h
 * 
 * A global hashtable for looking up full pathnames.
 */

#ifndef __PATH_CACHE__
#define __PATH_CACHE__

#include "fs.h"
#include "fsemu.h"

#define PATH_CACHE_SIZE	61	// Prime number!

struct pcache_entry {
	struct dentry 	*dent;
	unsigned long	hash2;
};

struct pcache {
	struct pcache_entry data[PATH_CACHE_SIZE];
};

extern struct pcache *pcache;

int pcache_init(void);

struct dentry *pcache_lookup(const char *pathname);
void pcache_put(const char *path, struct dentry *dent);

#ifdef DEBUG
void db_pcache_dump(void);
#endif  // DEBUG

#endif  // __PATH_CACHE__