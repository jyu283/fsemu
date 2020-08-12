/**
 * fsemu/src/loadf.c
 * 
 * Loads a file in the underlying operating system into the fsemu.
 */

#include "fsemu.h"
#include "fs_syscall.h"
#include "util.h"
#include "fserror.h"

#include <stdio.h>
#include <unistd.h>

/**
 * Load the file in the actual operating system specified by ospath
 * into the emulator at emupath.
 */ 
int loadf(const char *ospath, const char *emupath)
{
	FILE *src = fopen(ospath, "r");
	if (!src) {
		perror("open");
		return -1;
	}

	int fd = fs_open(emupath);
	if (fd < 0) {
		fs_pstrerror(fd, "fs_open");
		return -1;
	}

	int nread = 0, nwritten = 0, ret;
	char buf[BUFSIZ];
	while ((ret = fread(buf, 1, BUFSIZ, src))) {
		nread += ret;
		ret = fs_write(fd, buf, ret);
		if (ret < 0) {
			fs_pstrerror(ret, "write");
			return -1;
		}
		nwritten += ret;
	}
	fs_close(fd);
	fclose(src);

	printf("Total: %d bytes read, %d bytes written.\n", nread, nwritten);
	return 0;
}