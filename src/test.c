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
	printf("BSIZE = %d. DENTPERBLK = %ld.\n", BSIZE, DENTPERBLK);

	int ret;
	if ((ret = fs_mkdir("/usr")) < 0)
		fs_pstrerror(ret, "mkdir /usr");
	if ((ret = fs_creat("/test1")) < 0)
		fs_pstrerror(ret, "creat test1");
	if ((ret = fs_creat("/usr/test2")) < 0)
		fs_pstrerror(ret, "creat /usrtest2");
	if ((ret = fs_creat("/usr/test3")) < 0)
		fs_pstrerror(ret, "creat /usr/test3");
	if ((ret = fs_rmdir("/usr/local")) < 0)
		fs_pstrerror(ret, "rmdir /usr/local");
	if ((ret = fs_rmdir("/usr")) < 0)
		fs_pstrerror(ret, "rmdir /usr");
	if ((ret = fs_rmdir("/usr/test2")) < 0)
		fs_pstrerror(ret, "rmdir /usr/test2");
	if ((ret = fs_link("/usr/test3", "/link_test3")) < 0)
		fs_pstrerror(ret, "link /usr/test3 /link_test3");

	ls("/");
	ls("/usr");

	lsfd();
	printf("==========>>>>> END DEBUG <<<<<==========\n\n");
}