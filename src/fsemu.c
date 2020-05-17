/**
 * fsemu/src/fsemu.c
 * 
 * FSEMU: A user space File System EMUlator
 * 
 * This emulator is used to explore the cache performance of different
 * file system designs.
 * Created by: Jerry Yu and Abigail Matthews
 * Copyright (c) 2020 Arpaci-Dusseau Systems Lab. (Fancy!)
 */

#include "fs_syscall.h"
#include "util.h"
#include "fsemu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *fs = NULL;	// pointer to start of file system.
static size_t fs_size;	// size of file system.

/* File descriptors */
struct file openfiles[MAXOPENFILES];

/*
 * Processes instructions either from stdin or from a batch file.
 */
static void process(FILE *fp)
{

}

/*
 * Prints out help message.
 * Update this function as functionalities grow.
 */
static inline void print_help(void)
{
	puts("Usage: fsemu [input]");
	puts("Note: file input is currently not supported.");
}

int main(int argc, char *argv[])
{
	if (argc != 1) {
		print_help();
		exit(0);
	}

	fs_size = MAXFSSIZE;

	if (fs_mount(fs_size) < 0) {
		printf("Error: failed to mount file system.\n");
		exit(0);
	}

	/* Interactive mode only for now. */
	// process(stdin);

	test();
	sh();
	
	fs_unmount();
	return 0;
}