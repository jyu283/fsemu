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
	struct inode	*pi;
	unsigned long	hash2;
};

struct pcache {
	struct pcache_entry data[PATH_CACHE_SIZE];
};

extern struct pcache *pcache;

int pcache_init(void);
void pcache_free(void);

struct dentry *pcache_lookup(const char *pathname, struct inode **pi);
void pcache_put(const char *pathname, struct dentry *dent, struct inode *pi);

#ifdef DEBUG
void db_pcache_dump(void);
#endif  // DEBUG

#endif  // __PATH_CACHE__