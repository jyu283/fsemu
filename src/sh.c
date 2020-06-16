/**
 * fsemu/src/sh.c
 * 
 * An interactive shell. Maybe called bfsh -- Basic Filesystem Shell?
 * Pronounced beef shell... or buff shell? Oh decisions decisions.
 */

#include "fsemu.h"
#include "fs_syscall.h"
#include "fserror.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAXARGS		16	// probably won't ever need this many
#define PROMPTLEN	strlen(prompt)

static int argc;
static char *argv[MAXARGS];
static uint64_t sysargs[MAXARGS];

#define ARG_PTR	1
#define ARG_VAL	0

#define SYSCALL_DEFINE0(name) \
	static int sys_ ## name(void) {	\
		int ret;	\
		ret = fs_ ## name();	\
		return ret;	\
	}	

#define SYSCALL_DEFINE1(name, t1)  \
	static int sys_ ## name(void) {	\
		if (argc > 2) return -1;	\
		int ret;	\
		ret = fs_ ## name((t1)sysargs[0]);	\
		return ret;	\
	}

#define SYSCALL_DEFINE2(name, t1, t2)  \
	static int sys_ ## name(void) {	\
		int ret;	\
		ret = fs_ ## name((t1)sysargs[0], (t2)sysargs[1]);	\
		return ret;	\
	}

#define SYSCALL_DEFINE3(name, t1, t2, t3)  \
	static int sys_ ## name(void) {	\
		int ret;	\
		ret = fs_ ## name((t1)sysargs[0], (t2)sysargs[1], (t3)sysargs[2]);	\
		return ret;	\
	}

SYSCALL_DEFINE1(mount, unsigned long);
SYSCALL_DEFINE0(unmount);
SYSCALL_DEFINE1(open, const char *);
SYSCALL_DEFINE1(close, int);
SYSCALL_DEFINE1(unlink, const char *);
SYSCALL_DEFINE2(link, const char *, const char *);
SYSCALL_DEFINE1(mkdir, const char *);
SYSCALL_DEFINE1(rmdir, const char *);
SYSCALL_DEFINE1(creat, const char *);
SYSCALL_DEFINE2(rename, const char *, const char *);
SYSCALL_DEFINE2(lseek, int, unsigned int);
SYSCALL_DEFINE3(read, int, void *, unsigned int);
SYSCALL_DEFINE3(write, int, void *, unsigned int);

static int (*syscalls[])(void) = {
	[SYS_mount]		sys_mount,
	[SYS_unmount]	sys_unmount,
	[SYS_open]		sys_open,
	[SYS_close]		sys_close,
	[SYS_unlink]	sys_unlink,
	[SYS_link]		sys_link,
	[SYS_mkdir]		sys_mkdir,
	[SYS_rmdir]		sys_rmdir,
	[SYS_creat]		sys_creat,
	[SYS_lseek]		sys_lseek,
	[SYS_read]		sys_read,
	[SYS_write]		sys_write
};

static const char *prompt = "(fsemu) ";

/**
 * Prints prompt string to stdout.
 */
static inline void print_prompt(void)
{
	fflush(stdout);
	write(STDOUT_FILENO, prompt, PROMPTLEN);
}

/**
 * Map a system call name to a system call ID. (see fs_syscall.h)
 */
static int get_sysnum(const char *name)
{
#define REGISTER_SYSCALL(x) \
	if (strcmp(name, #x) == 0)  \
		return SYS_ ## x;

	REGISTER_SYSCALL(mount);
	REGISTER_SYSCALL(unmount);
	REGISTER_SYSCALL(open);
	REGISTER_SYSCALL(close);
	REGISTER_SYSCALL(unlink);
	REGISTER_SYSCALL(link);
	REGISTER_SYSCALL(mkdir);
	REGISTER_SYSCALL(rmdir);
	REGISTER_SYSCALL(creat);
	REGISTER_SYSCALL(rename);
	REGISTER_SYSCALL(lseek);
	REGISTER_SYSCALL(read);
	REGISTER_SYSCALL(write);

	return -1;
}

static int syscall_handler(void)
{
#define if_syscall(x)	if (strcmp(argv[0], #x) == 0)
#define check_argc(x)	if (argc != x+1) ret = -EARGS; goto out;
#define SYSCALL_ARGINT(x)	sysargs[x] = (uint64_t)atol(argv[x+1])
#define SYSCALL_ARGPTR(x)	sysargs[x] = (uint64_t)argv[x+1] 

	int ret;
	int sysnum = get_sysnum(argv[0]);	
	if (sysnum < 0) {
		ret = -ECMD;
		goto out;
	} 

	if_syscall(mount) {
		check_argc(1);
		SYSCALL_ARGINT(0);
	} else if_syscall(unmount) {
		check_argc(0)
	} else if_syscall(open) {
		check_argc(1);
		SYSCALL_ARGPTR(0);
	} else if_syscall(close) {
		check_argc(1);
		SYSCALL_ARGINT(0);
	} else if_syscall(unlink) {
		check_argc(1);
		SYSCALL_ARGPTR(0);
	} else if_syscall(link) {
		check_argc(2);
		SYSCALL_ARGPTR(0);
		SYSCALL_ARGPTR(1);
	} else if_syscall(mkdir) {
		check_argc(1);
		SYSCALL_ARGPTR(0);
	} else if_syscall(rmdir) {
		check_argc(1);
		SYSCALL_ARGPTR(0);
	} else if_syscall(creat) {
		check_argc(1);
		SYSCALL_ARGPTR(0);
	} else if_syscall(rename) {
		check_argc(2);
		SYSCALL_ARGPTR(0);
		SYSCALL_ARGPTR(1);
	} else {
		ret = -ECMD;
		goto out;
	}

	// Execute system call.
	ret = syscalls[sysnum]();

out:
	if (ret < 0) {
		fs_pstrerror(ret, argv[0]);
		ret = -1;
	}
	return ret;
}

/**
 * Handles cat command.
 */
static void cat_handler()
{
	if (argc == 1) {
		printf("Usage: cat file\n");
		return;
	}

	int ret;
	for (int i = 1; i < argc; i++) {
		if ((ret = cat(argv[i])) < 0) {
			fs_pstrerror(ret, "cat");
		}
	}
}

/**
 * Handle ls command, as the function name very concisely and
 * precisely conveys.
 */
static void ls_handler()
{
	if (argc == 1) {
		ls("/");  // This will become cwd
	} else {
		for (int i = 1; i < argc; i++) {
			if (ls(argv[i]) < 0) {
				fs_pstrerror(ENOFOUND, "ls");
			}
		}
	}
}

/**
 * Process a list of arguments broken down by process()
 */
static int process_args()
{
	// Handle "user programs" such as ls
	if (strcmp(argv[0], "ls") == 0) {
		ls_handler(argc, argv);
		return 0;
	} else if (strcmp(argv[0], "cat") == 0) {
		cat_handler(argc, argv);
		return 0;
	}

	// system call handling
	syscall_handler();

	return 0;
}

/**
 * Process one line of user input.
 * 
 * Returns -1 when quit command is issued by user.
 */
int process(char *line)
{
	argv[0] = NULL;
	argc = 0;

	// Empty input (only '\n')
	if (strlen(line) == 1)
		return 0;
	line[strlen(line) - 1] = '\0';	// trim trailing newline char

	argv[argc++] = strtok(line, " \t");
	if (!argv[0])
		return 0;
	if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "q") == 0)
		return -1;

	char *arg = NULL;
	while ((arg = strtok(NULL, " \t"))) {
		if (argc >= MAXARGS) {
			printf("Error: maximum %d arguments.\n", MAXARGS);
			return 0;
		}
		argv[argc++] = arg;
	}

	process_args(argc, argv);

	return 0;
}

/**
 * Interactive shell
 */
void sh(void)
{
	char *line = NULL;
	size_t len = 0;

	print_prompt();
	while (getline(&line, &len, stdin) != -1) {
		if (process(line) < 0)
			break;
		print_prompt();
	}

	free(line);
}
