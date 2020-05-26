/**
 * fsemu/include/fs_syscall.h 
 * 
 * Available I/O system calls.
 * The hope is to one day make this file system completely
 * POSIX-compliant. Of course that begs the question: What on 
 * Earth is the flipping point? To which I will not have an 
 * answer, for I, too, have no idea why I am doing this. But
 * it's good fun, and perhaps that's reason enough. This 
 * whole thing is really, in its nature, more or less a DIY
 * pastime.
 * 
 * Jerry
 * 14 May 2020, 1:03 AM
 * 
 * P.S. I should go to bed...
 */

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "fsemu.h"

int fs_mount(unsigned long size);
int fs_unmount(void);
int fs_open(const char *pathname);
int fs_close(int fd);
int fs_unlink(const char *pathname);
int fs_link(const char *oldpath, const char *newpath);
int fs_mkdir(const char *pathname);
int fs_rmdir(const char *pathname);
int fs_creat(const char *pathname);
unsigned int fs_lseek(int fd, unsigned int off);
unsigned int fs_read(int fd, void *buf, unsigned int count);
unsigned int fs_write(int fd, void *buf, unsigned int count);

/* system call IDs */

#define SYS_mount	0
#define SYS_unmount	1
#define SYS_open	2
#define SYS_close	3
#define SYS_unlink	4
#define SYS_link	5
#define SYS_mkdir	6
#define SYS_rmdir	7
#define SYS_creat	8
#define SYS_lseek	9
#define SYS_read	10
#define SYS_write	11

/* 
 * System call handler functions, where arguments are retrieved.
 * 
 * Handling arguments:
 * Obivously arguments can't be directly passed through system calls,
 * so some assembly manipulation is required.
 */

// mapping from syscall ID to system call handlers, 
// in which arguments are retrieved.
/*
static int (*syscalls[])(void) = {
	[SYS_mount]		sys_mount,
	[SYS_unmount]	sys_unmount,
	[SYS_open]		sys_open,
	[SYS_close]		sys_close,
	[SYS_unlink]	sys_unlink,
	[SYS_link]		sys_link,
	[SYS_mkdir]		sys_mkdir,
	[SYS_rmdir]		sys_rmdir,
	[SYS_creat]		sys_creat,
	[SYS_lseek]		sys_lseek,
	[SYS_read]		sys_read,
	[SYS_write]		sys_write
};
*/

// Debug functions
// If around declarations because these functions should
// only be called when debugging. 
#ifdef DEBUG
void test(void);
#endif


#endif  // __SYSCALL_H__