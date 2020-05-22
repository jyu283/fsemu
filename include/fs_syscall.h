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

// Debug functions
// If around declarations because these functions should
// only be called when debugging. 
#ifdef DEBUG
int db_creat_at_root(const char *name, uint8_t type);
int db_mkdir_at_root(const char *name);
void test(void);
#endif


#endif  // __SYSCALL_H__