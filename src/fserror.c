/**
 * fsemu/src/fserror.c
 * 
 * Error codes.
 * 
 * For now this is really just a number->string translation.
 * More will be done here once there is an actual simulated 
 * "system call" process.
 */

#include "fserror.h"

#include <stdio.h>

static const char *errors[] = {
	[0]			"No error",
	[ENOFOUND]	"No such file or directory",
	[EEXISTS]	"File or directory already exists",
	[EALLOC]	"Allocation failed",
	[EINVTYPE]	"Invalid file type",
	[EINVNAME]	"Invalid name",
	[ENOFD]		"No available file descriptor",
	[EINVFD]	"Invalid file descriptor",
	[ENOTEMPTY]	"Directory is not empty",
	[EINVFD]	"Invalid parameter"
};

/**
 * Returns a string explaining the error code.
 */
const char *fs_strerror(int errno)
{
	static int maxerrno;
	maxerrno = sizeof(errors) / sizeof(errors[0]);
	if (errno > 0 || errno < -maxerrno) {
		return "strerror: bad error code";
	} else {
		return errors[-errno];
	}
}

/**
 * Prints out a string for the error code after a title.
 */
void fs_pstrerror(int errno, const char *title)
{
	printf("%s: %s.\n", title, fs_strerror(errno));
}