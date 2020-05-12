#ifndef __FSEMU_H__
#define __FSEMU_H__

#include <file.h>

#include <stdio.h>

#define DEBUG   

#ifdef DEBUG
#define pr_debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define pr_debug(...) do { } while (0)
#endif

#define MAXOPENFILES    32

extern char *fs;
extern struct file openfiles[MAXOPENFILES];

#endif  // __FSEMU_H__