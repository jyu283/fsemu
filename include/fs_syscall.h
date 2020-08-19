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

#include <time.h>

struct hfs_stat {
	uint32_t	st_ino;
	uint32_t	st_size;
	uint32_t	st_blocks;
	uint32_t	st_nlink;
	uint32_t	st_type;
	time_t		st_accesstime;
	time_t		st_modifytime;
	time_t		st_changetime;
};

int fs_mount(unsigned long size);
int fs_unmount(void);
int fs_open(const char *pathname);
int fs_close(int fd);
int fs_unlink(const char *pathname);
int fs_link(const char *oldpath, const char *newpath);
int fs_mkdir(const char *pathname);
int fs_rmdir(const char *pathname);
int fs_creat(const char *pathname);
int fs_rename(const char *oldpath, const char *newpath);
unsigned int fs_lseek(int fd, unsigned int off);
unsigned int fs_read(int fd, void *buf, unsigned int count);
unsigned int fs_write(int fd, void *buf, unsigned int count);
int fs_reset(void);
int fs_symlink(const char *target, const char *linkpath);
int fs_readlink(const char *pathname, char *buf, size_t bufsize);
int fs_stat(const char *pathname, struct hfs_stat *statbuf);
int fs_chdir(const char *pathname);

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
#define SYS_rename	12
#define SYS_reset   13
#define SYS_symlink	14
#define SYS_readlink 15
#define SYS_stat	16
#define SYS_chdir	17

// Debug functions
// If around declarations because these functions should
// only be called when debugging. 
#ifdef DEBUG
void test(void);
#endif


#endif  // __SYSCALL_H__