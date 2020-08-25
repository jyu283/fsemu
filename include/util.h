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
int cat(const char *pathname);
int readl(const char *pathname);
int loadf(const char *ospath, const char *emupath);
int filestat(const char *pathname);

int benchmark_init_fs(const char *input_file);
int benchmark_lookup(const char *input_file, int repcount);
void benchmark(const char *input_file);

#ifdef HFS_DEBUG
void show_inline(void);
void show_regular(void);
void hfs_dirhash_dump(void);
void hfs_dirhash_clear(void);
#endif  // HFS_DEBUG

#endif  // __UTIL_H__