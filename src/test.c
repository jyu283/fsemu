/**
 * fsemu/src/test.c
 * 
 * This file is used for testing.
 */

#include "fsemu.h"
#include "fserror.h"
#include "fs_syscall.h"
#include "util.h"
#include "dentry_cache.h"

void test(void)
{
	benchmark("input_file.txt");
}