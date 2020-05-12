#ifndef __FSEMU_H__
#define __FSEMU_H__

#include <stdio.h>

#define DEBUG   

#ifdef DEBUG
#define pr_debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define pr_debug(...) do { } while (0)
#endif

extern char *fs;

#endif  // __FSEMU_H__