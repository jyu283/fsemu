/**
 * fsemu/include/file.h
 * 
 * Defines the "file" construct, file descriptors etc.
 */

#ifndef __FILE_H__
#define __FILE_H__

#include "fs.h"

#include <stdint.h>

#define MAXOPENFILES    32

typedef unsigned int	foff_t;     // R/W offset value

/**
 * The file structure represents an open file. In real Linux, 
 * open files are private to each process. Since there are on
 * processes in this emulator (yet), we will consider open files
 * a property of the emulator itself.
 */
struct file {
    foff_t          offset;
	struct hfs_dentry	*f_dentry;
};

extern struct file openfiles[MAXOPENFILES];

#endif  // __FILE_H__