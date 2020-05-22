/**
 * fsemu/include/fserror.h
 * 
 * Error codes.
 */

#ifndef __FSERROR_H__
#define __FSERROR_H__

#define ENOFOUND	1   // File not found
#define EEXISTS		2   // File already exists
#define EALLOC		3   // Allocation failure
#define EINVTYPE	4   // Invalid inode type
#define EINVNAME	5   // Invalid name
#define ENOFD		6   // No available file descriptors
#define EINVFD		7   // Invalid file descriptor
#define ENOTEMPTY	8   // Directory not empty (rmdir)
#define EINVAL      9   // A system call parameter is invalid

const char *fs_strerror(int errno);
void fs_pstrerror(int errno, const char *title);

#endif  // __FSERROR_H__