#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "fs.h"
#include "fsemu.h"

int fs_open(const char *pathname);
int fs_close(int fd);

// Debug functions
#ifdef DEBUG
int db_creat_at_root(const char *name, uint8_t type);
int db_mkdir_at_root(const char *name);
#endif


#endif  // __SYSCALL_H__