/**
 * fsemu/include/fsemu.h
 * 
 * Things relevant to the emulator itself. Strictly "user space".
 * For now the fsemu itself is also a "process" so open file 
 * descriptors can also be found here.
 */

#ifndef __FSEMU_H__
#define __FSEMU_H__

#include "file.h"

#include <stdio.h>

#define DEBUG   

#ifdef DEBUG
#define pr_debug(...) \
	do {	\
		fprintf(stderr, "DEBUG: (%s, %d): %s: ", 	\
				__FILE__, __LINE__, __func__);	\
		fprintf(stderr, __VA_ARGS__);	\
	} while(0)
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

// Interactive shell (fsemu/src/sh.c)
void sh(void);

#endif  // __FSEMU_H__