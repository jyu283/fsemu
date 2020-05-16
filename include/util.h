/**
 * fsemu/include/util.h
 * 
 * Declared herein are pseudo "user programs" related
 * to the file system.
 */ 

#ifndef __UTIL_H__
#define __UTIL_H__

#include "fsemu.h"

// Pseudo-user programs
int ls(const char *pathname);
void lsfd(void);

#endif  // __UTIL_H__