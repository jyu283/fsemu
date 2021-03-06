/**
 * fsemu/include/fsemu.h
 * 
 * Things relevant to the emulator itself. Strictly "user space".
 * For now the fsemu itself is also a "process" so open file 
 * descriptors can also be found here.
 */

#ifndef __FSEMU_H__
#define __FSEMU_H__

#include <stdio.h>
#include <stdint.h>

#define HFS_DEBUG   

#define MAXFSSIZE   0x40000000

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define KBLD  "\x1B[1m"

#ifdef HFS_DEBUG
#define pr_info(...) \
	do {	\
		fprintf(stderr, __VA_ARGS__);	\
	} while (0)

#define pr_debug(...) \
	do {	\
		fprintf(stderr, KBLD KYEL); \
		fprintf(stderr, "DEBUG: (%s, %d): %s: ", 	\
				__FILE__, __LINE__, __func__);	\
		fprintf(stderr, KNRM); \
		fprintf(stderr, __VA_ARGS__);	\
	} while (0)

#define pr_warn(...)\
    do {    \
        fprintf(stderr, KBLD KRED);  \
		fprintf(stderr, "WARNING: (%s, %d): %s: ", 	\
				__FILE__, __LINE__, __func__);	\
        fprintf(stderr, KNRM); \
		fprintf(stderr, __VA_ARGS__);   \
    } while (0)

#else
#define pr_info(...)	do { } while (0)
#define pr_debug(...)   do { } while (0)
#define pr_warn(...)    do { } while (0) 
#endif

// Interactive shell (fsemu/src/sh.c)
void sh(FILE *fp);

#endif  // __FSEMU_H__
