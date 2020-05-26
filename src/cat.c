/**
 * fsemu/src/cat.c
 * 
 * A basic cat program. Right now this program is directly treated
 * as a system call and is therefore technically "in kernel" and
 * has direct access to inodes and whatnot. 
 */

#include "fs.h"
#include "fs_syscall.h"
#include "util.h"
#include "fsemu.h"

#include <stdio.h>

#define BUFFSZ	512

int cat(const char *pathname)
{
	int fd = fs_open(pathname);
	if (fd < 0)
		return fd;

	char buf[BUFFSZ];	
	int ret;
	while ((ret = fs_read(fd, buf, BUFFSZ)) > 0) {
		printf("%s", buf);
	}
	return ret;
}