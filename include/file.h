#ifndef __FILE_H__
#define __FILE_H__

#include "fs.h"

#include <stdint.h>

typedef uint64_t	foff_t;     // R/W offset value

/**
 * The file structure represents an open file. In real Linux, 
 * open files are private to each process. Since there are on
 * processes in this emulator (yet), we will consider open files
 * a property of the emulator itself.
 */
struct file {
    foff_t          offset;
	struct dentry	*f_dentry;
};

#endif  // __FILE_H__