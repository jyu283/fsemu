#ifndef __UTIL_H__
#define __UTIL_H__

#include "fs.h"
#include "fsemu.h"

// File system related functions
void init_fs(size_t size);

// Pseudo-user programs
int ls(void);

// Debug functions
#ifdef DEBUG
int db_creat_at_root(const char *name, uint8_t type);
int db_mkdir_at_root(const char *name);
#endif

#endif  // __UTIL_H__