/*
 * FSEMU: A user space File System EMUlator
 * 
 * This emulator is used to explore the cache performance of different
 * file system designs.
 * Created by: Jerry Yu and Abigail Matthews
 * Copyright (c) 2020 Arpaci-Dusseau Systems Lab. (Fancy!)
 */

#include "fs.h"
#include "fsemu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *fs = NULL;

/*
 * Allocates space for file system in memory.
 */
static void alloc_fs(size_t size)
{
    pr_debug("Allocating %lu bytes for file system...\n", size);

    int fd;
    if (size > MAXFSSIZE) {
        printf("Error: file system size cannot be greater than 1GB.\n");
        exit(0);
    }
    if ((fd = open("/dev/zero", O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }
    fs = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (!fs) {
        perror("mmap");
        exit(1);
    }
    close(fd);

    init_superblock(size);
}

/*
 * Processes instructions either from stdin or from a batch file.
 */
static void process(FILE *fp)
{

}

/*
 * Prints out a pretentious preamble.
 */ 
static inline void print_title(void)
{
    puts("FSEMU (File System EMUlator) 0.01-05.11.20");
    puts("Copyright (C) 2020 Arpaci-Dusseau Systems Lab");
    puts("License GPLv3+: GNU GPL version 3 or later");
    puts("Type \"help\" for help.");
    puts("Type \"quit\" to quit.");
    puts("");
}

/*
 * Prints out help message.
 * Update this function as functionalities grow.
 */
static inline void print_help(void)
{
    puts("Usage: fsemu");
}

int main(int argc, char *argv[])
{
    if (argc != 1) {
        print_help();
        exit(0);
    }

    print_title();
    alloc_fs(MAXFSSIZE);  // 1GB fixed-sized for now.

    /* Interactive mode only for now. */
    process(stdin);
    munmap(fs, MAXFSSIZE);

    return 0;
}