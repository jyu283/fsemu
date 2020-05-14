/*
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
	char filename[DENTRYNAMELEN];
	for (int i = 0; i < 10; i++) {
		sprintf(filename, "testfile%d", i);
		db_creat_at_root(filename, T_REG);
	}
	for (int i = 0; i < 5; i++) {
		sprintf(filename, "device%d", i);
		db_creat_at_root(filename, T_DEV);
	}
	db_creat_at_root("opentest", T_REG);
	for (int i = 0; i < 10; i++) {
		sprintf(filename, "testdir%d", i);
		db_creat_at_root(filename, T_DIR);
	}
	ls();
	int fd = fs_open("opentest");
	if (fd < 0) {
		printf("Failed to open file.\n");
	}
	lsfd();
	fs_close(fd);
	printf("Removing testfile0 and device 0...\n");
	fs_unlink("testfile0");
	fs_unlink("device0");
	ls();
}

/*
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

	print_title();

	fs_size = MAXFSSIZE;

	if (fs_mount(fs_size) < 0) {
		printf("Error: failed to mount file system.\n");
		exit(0);
	}

	/* Interactive mode only for now. */
	process(stdin);
	
	fs_unmount();
	return 0;
}