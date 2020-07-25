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

static size_t fs_size;	// size of file system.

/* File descriptors */
struct file openfiles[MAXOPENFILES];

/**
 * Prints out a pretentious preamble.
 */ 
static inline void print_title(void)
{
	puts("FSEMU (File System EMUlator) 0.01-05.11.20");
	puts("Copyright (C) 2020 Arpaci-Dusseau Systems Lab");
	puts("License GPLv3+: GNU GPL version 3 or later");
	puts("Type \"help\" for help. Type \"quit\" to quit.");
	puts("");
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2) {
		exit(0);
	}

	fs_size = MAXFSSIZE;

	print_title();

	if (fs_mount(fs_size) < 0) {
		printf("Error: failed to mount file system.\n");
		exit(0);
	}

	// test();
	FILE *shell_fp;
	if (argc == 2) {
		if (!(shell_fp = fopen(argv[1], "r"))) {
			perror("open");
			exit(1);
		}
		printf("Starting shell in batch mode...\n");
	} else {
		shell_fp = stdin;
		printf("Starting shell in interactive mode...\n");
	}

	sh(shell_fp);
	
	fs_unmount();
	return 0;
}