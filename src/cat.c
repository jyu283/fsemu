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
#include "fserror.h"
#include "fsemu.h"

#include <stdio.h>
#include <unistd.h>

#define BUFFSZ		512
#define LINKBUFSZ	4096

int cat(const char *pathname)
{
	int fd = fs_open(pathname);
	if (fd < 0)
		return fd;

	char buf[BUFFSZ];	
	int ret;
	while ((ret = fs_read(fd, buf, BUFFSZ)) > 0) {
		if (write(1, buf, ret) < 0) {
			perror("write");
			return -1;
		}
	}
	if (ret < 0) 
		fs_pstrerror(ret, "cat");

	fs_close(fd);
	return ret;
}

int readl(const char *pathname)
{
	char link[LINKBUFSZ];
	return fs_readlink(pathname, link, LINKBUFSZ);
}