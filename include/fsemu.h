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

/* 
 * Plan to simulate processes with threads. By which time
 * things like the openfiles array should be a property of
 * each individual process. Right now we consider the entire
 * emulator as a single process -- which of course it is but
 * in a difference sense.
 */

extern struct file openfiles[MAXOPENFILES];

#endif  // __FSEMU_H__