/**
 * fsemu/src/io_util.c
 * 
 * Generic I/O utilities for HFS.
 */

#include "util.h"
#include "fsemu.h"
#include "fs_syscall.h"
#include "fserror.h"

/* The need to include fs.h should be eliminated with further implementation
 * of more system calls such as getdents(). */
#include "fs.h"

#include <unistd.h>
#include <stdio.h>

#define LINKBUFSZ	4096

/**
 * cat - concatenate files and print on the standard output.
 */
int cat(const char *pathname)
{
	int fd = fs_open(pathname);
	if (fd < 0)
		return fd;

	char buf[BUFSIZ];	
	int ret;
	while ((ret = fs_read(fd, buf, BUFSIZ)) > 0) {
		if (write(1, buf, ret) < 0) {
			perror("write");
			return -1;
		}
	}
	if (ret < 0) 
		fs_pstrerror(ret, "cat");

	fs_close(fd);
	return ret;
}

/**
 * readl (readlink) - print resolved symbolic links or canonical file names.
 */
int readl(const char *pathname)
{
	char link[LINKBUFSZ];
	return fs_readlink(pathname, link, LINKBUFSZ);
}

static char *type_names[] = {
	[T_UNUSED]  = "UNUSED",
	[T_REG]     = "FILE  ",
	[T_DIR]     = "DIR   ",
	[T_DEV]     = "DEVICE",
	[T_SYM]		= "LINK  ",
};

static const char *month_names[] = 
{
	[0] = "Jan", [1] = "Feb", [2] = "Mar", [3] = "Apr", [4] = "May",
	[5] = "Jun", [6] = "Jul", [7] = "Aug", [8] = "Sep", [9] = "Oct",
	[10] = "Nov", [11] = "Dec", 
};

/**
 * Format: Mon DD hh:mm
 */
static inline void time_str(time_t *timep, char *timestr)
{
	struct tm *t = localtime(timep);
	sprintf(timestr, "%s %2d %02d:%02d", month_names[t->tm_mon],
					t->tm_mday, t->tm_hour, t->tm_min);
}

/**
 * Print out formatted inode metadata.
 * File name colours: 
 * 	 FILE (T_REG) = white
 *   DIR  (T_DIR) = blue + bold
 *   LINK (T_SYM) = cyan + bold
 */
static inline void print_inode(struct hfs_inode *inode, const char *name)
{
	static char mtime[13];
	time_str(&inode->mtime, mtime);
	printf("%s %-3d ", type_names[inode->type], inode->nlink);
	if (inode->type == T_DIR)
		printf(KBLD KBLU);
	else if (inode->type == T_SYM)
		printf(KBLD KCYN);
	printf("%-16s " KNRM, name);
	printf("%-6d %6d %s\n", inode->size, inum(inode), mtime);
}

static inline void print_dentry(struct hfs_dentry *dent, char *off_start)
{
	struct hfs_inode *inode = dentry_get_inode(dent);
	unsigned long off = (char *)dent - off_start;

	print_inode(inode, dentry_get_name(dent));
	// printf("rlen=%-3d nlen=%-3d off=%lu-%lu\n", dent->reclen, 
	// 					dent->namelen, off, off + dent->reclen);
}

/**
 * Print out a block of dentries in something resembling 
 * the following format:
 * (Does not print out unused dentries)
 */
static inline void print_dentry_block(uint32_t block_num)
{
	char *block = BLKADDR(block_num);
	struct hfs_dentry *dent;
	struct hfs_inode *inode;

	for_each_block_dent(dent, block) {
		if (dent->reclen == 0)
			break;
		if (dent->inum)
			print_dentry(dent, block);
	}
}

/*
 * Perform ls on a given directory
 */
static void ls_dir(struct hfs_inode *dir, const char *name)
{
	printf("%s \n", name);
	if (dir->type != T_DIR)
		return;

#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		printf("[Inline directory]\n");
		struct hfs_dentry *inline_dent;
		print_inode(dir, ".");
		print_inode(&inodes[dir->data.inline_dir.p_inum], "..");
		for_each_inline_dent(inline_dent, dir) {
			if (!inline_dent->reclen)
				break;
			if (inline_dent->inum) {
				print_dentry(inline_dent, (char *)&dir->data.inline_dir);
			}
		}
		return;
	} 
#endif
	for (int i = 0; i < NBLOCKS; i++) {
		if (dir->data.blocks[i]) {
			print_dentry_block(dir->data.blocks[i]);
		}
	}
	
}

/**
 * ls - list directory contents
 * NOTE: This should be re-implemented once additional system calls
 * such as getdents have been implemented.
 */
int ls(const char *pathname)
{
	struct hfs_inode *src;
	const char *name;
	if (!pathname) {
		src = dentry_get_inode(cwd);
		name = "";
	} else {
		struct hfs_dentry *src_dent;
		if (!(src_dent = lookup(pathname)))
			return -1;
		src = dentry_get_inode(src_dent);
		name = dentry_get_name(src_dent);
	}

	// For now only prints out the root's contents
	ls_dir(src, name);
	puts("");
	pr_info("Total inodes in use: %lu\n", sb->inode_used);
	pr_info("Directories: %lu total. Files: %lu total.\n", 
							sb->ndirectories, sb->nfiles);
	pr_info("Total inline inodes: %lu\n", sb->inline_inodes);
	return 0;
}

/**
 * List all open file descriptors.
 */
void lsfd(void)
{
	printf("open file descriptors: \n");
	for (int i = 0; i < MAXOPENFILES; i++) {
		if (openfiles[i].f_dentry) {
			printf(" [%d] %s (off=%d)\n", i, 
					dentry_get_name(openfiles[i].f_dentry),
					openfiles[i].offset);
		}
	}
	puts("");
}

#ifdef HFS_DEBUG
static int do_show_inline(struct hfs_inode *dir)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (inode_is_inline_dir(dir)) {
		ls_dir(dir, "#####");
		return 0;
	} else {
		struct hfs_inode *inode;
		struct hfs_dentry *dent;
		char *block;
		for (int i = 0; i < NBLOCKS; i++) {
			if (dir->data.blocks[i] == 0)
				continue;
			block = BLKADDR(dir->data.blocks[i]);
			for_each_block_dent(dent, block) {
				if (!dent->reclen)
					break;
				if (dent->inum == 0)
					continue;
				if (strcmp(dent->name, ".") == 0 || 
						strcmp(dent->name, "..") == 0)
					continue;
				inode = inode_from_inum(dent->inum);
				if (inode->type != T_DIR)
					continue;
				if (do_show_inline(inode) == 0)
					return 0;
			}	
		}
	}
#endif
	return -1;
}

/**
 * Debug function:
 * Show inline directories.
 */
void show_inline(void)
{
	do_show_inline(get_root_inode());
}

static int do_show_regular(struct hfs_inode *dir)
{
#ifdef _HFS_INLINE_DIRECTORY
	if (!inode_is_inline_dir(dir)) {
		ls_dir(dir, "#####");
		return 0;
	} else {
		struct hfs_inode *inode;
		struct hfs_dentry *dent;
		for_each_inline_dent(dent, dir) {
			if (!dent->reclen)
				break;
			if (dent->inum == 0)
				continue;
			inode = inode_from_inum(dent->inum); 
			if (inode->type != T_DIR)
				continue;
			if (do_show_regular(inode) == 0)
				return 0;
		}
	}
#endif
	return -1;
}

/**
 * Debug function:
 * Show regular directories.
 */
void show_regular(void)
{
	do_show_regular(get_root_inode());
}

#endif  // HFS_DEBUG

/**
 * loadf - load a local file's contents into a file in the emulator.
 * 
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

/**
 * Inode type name strings used by filestat command.
 */
static char *stat_type_names[] = {
	[T_REG]     = "regular file",
	[T_DIR]     = "directory",
	[T_DEV]     = "device",
	[T_SYM]		= "symbolic link",
};

/**
 * filestat (istat) - display file status
 * 
 * filestat [path] command.
 * stat() a file and print out the metadata.
 */
int filestat(const char *pathname)
{
	int ret;
	struct hfs_stat statbuf;
	if ((ret = fs_stat(pathname, &statbuf)) < 0) {
		fs_pstrerror(ret, "stat");
		return -1;
	}

	printf("File:  %s\n", pathname);
	printf("Size:  %-15d Blocks: %-7d\n", statbuf.st_size, statbuf.st_blocks);
	printf("Inode: %-15d Links:  %-7d  ", statbuf.st_ino, statbuf.st_nlink);
	printf("%s\n", stat_type_names[statbuf.st_type]);
	printf("Access: %s", ctime(&statbuf.st_accesstime));
	printf("Modify: %s", ctime(&statbuf.st_modifytime));
	printf("Change: %s", ctime(&statbuf.st_changetime));
	return 0;
}
