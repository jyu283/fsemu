#ifndef __UTIL_H__
#define __UTIL_H__

#include "fs.h"
#include "fsemu.h"

// File system related functions
void init_fs(size_t size);

// Pseudo-user programs
int ls(void);
void lsfd(void);

#endif  // __UTIL_H__