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

/* SYSCALL_DEFINEx(name)
 * Defines a system call handler sys_name(), which must then be
 * added to the syscalls[] array.
 * Inspired by the Linux kernel version of these macros, albeit 
 * implemented with much less elegance. The number x indicates 
 * the number of arguments tihs system call has, and t1, t2, t3
 * represents the types of each of these arguments.
 */ 

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
SYSCALL_DEFINE0(reset);
SYSCALL_DEFINE2(symlink, const char *, const char *);

static int (*syscalls[])(void) = {
	[SYS_mount]		= sys_mount,
	[SYS_unmount]	= sys_unmount,
	[SYS_open]		= sys_open,
	[SYS_close]		= sys_close,
	[SYS_unlink]	= sys_unlink,
	[SYS_link]		= sys_link,
	[SYS_mkdir]		= sys_mkdir,
	[SYS_rmdir]		= sys_rmdir,
	[SYS_creat]		= sys_creat,
	[SYS_lseek]		= sys_lseek,
	[SYS_read]		= sys_read,
	[SYS_write]		= sys_write,
	[SYS_rename]	= sys_rename,
	[SYS_reset]		= sys_reset,
	[SYS_symlink]	= sys_symlink,
};

static const char *prompt = "(fsemu) ";

/**
 * Prints prompt string to stdout.
 */
static inline void print_prompt(void)
{
	fflush(stdout);
	printf("%s", prompt);
	fflush(stdout);
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
	REGISTER_SYSCALL(reset);
	REGISTER_SYSCALL(symlink);

	return -1;
}

/**
 * Checks argv[0] and calls the appropriate system call.
 * First use the string to determine the corresponding system call ID;
 * then parse the remainder of argv according to the parameters of each
 * system call. 
 *   - SYSCALL_ARGINT(x): Parse argv[x] as a numeric value
 * 	 - SYSCALL_ARGPTR(x): Parse argv[x] as a pointer (to a string)
 * If it is a valid system call (and one supported by the shell -- read
 * and write are not supported directly), use the system call number to
 * index into the system call table to call the appropriate function. 
 * 
 * Note: In order for the system call table to work, a function pointer
 * must be added to the syscalls[] array, and a corresponding 
 * SYSCALL_DEFINEx macro must be present.
 */
static int syscall_handler(void)
{
#define check_argc(x)	\
	do { if (argc != x+1) { ret = -EARGS; goto out; }} while (0)
#define SYSCALL_ARGINT(x)	sysargs[x] = (uint64_t)atol(argv[x+1])
#define SYSCALL_ARGPTR(x)	sysargs[x] = (uint64_t)argv[x+1] 

	int ret;
	int sysnum = get_sysnum(argv[0]);	
	if (sysnum < 0) {
		ret = -ECMD;
		goto out;
	} 

	switch (sysnum)
	{
	case SYS_mount: 
		check_argc(1);
		SYSCALL_ARGINT(0);
		break;
	case SYS_unmount:
		check_argc(0);
		break;
	case SYS_open:
		check_argc(1);
		SYSCALL_ARGPTR(0);
		break;
	case SYS_close:
		check_argc(1);
		SYSCALL_ARGINT(0);
		break;
	case SYS_unlink:
		check_argc(1);
		SYSCALL_ARGPTR(0);
		break;
	case SYS_link:
		check_argc(2);
		SYSCALL_ARGPTR(0);
		SYSCALL_ARGPTR(1);
		break;
	case SYS_mkdir:
		check_argc(1);
		SYSCALL_ARGPTR(0);
		break;
	case SYS_rmdir:
		check_argc(1);
		SYSCALL_ARGPTR(0);
		break;
	case SYS_creat:
		check_argc(1);
		SYSCALL_ARGPTR(0);
		break;
	case SYS_rename:
		check_argc(2);
		SYSCALL_ARGPTR(0);
		SYSCALL_ARGPTR(1);
		break;
	case SYS_reset:
		check_argc(0);
		break;
	case SYS_symlink:
		check_argc(2);
		SYSCALL_ARGPTR(0);
		SYSCALL_ARGPTR(1);
		break;
	default:
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
 * Change the shell's working directory
 */
static int cd(const char *path)
{
	int ret;
	if ((ret = fs_chdir(path)) < 0) {
		fs_pstrerror(ret, "cd");
		return -1;
	}
	return 0;
}

/**
 * Handles the cd [path] command.
 */
static void cd_handler(void)
{
	if (argc != 2) {
		printf("Usage: cd [path]\n");
		return;
	}
	cd(argv[1]);
}

/**
 * Handles the show_inline command.
 */
static void show_inline_handler()
{
#ifdef HFS_DEBUG
	show_inline();
#endif
}

/**
 * Handles the show_regular command.
 */
static void show_regular_handler()
{
#ifdef HFS_DEBUG
	show_regular();
#endif
}

static void dirhash_dump_handler()
{
#ifdef HFS_DEBUG
	hfs_dirhash_dump();
#endif
}

/**
 * Handles readl command (readlink)
 */
static void readl_handler()
{
	if (argc != 2) {
		printf("Usage: readl [LINK]\n");
		return;
	}
	readl(argv[1]);
}

/**
 * Handles "loadf [ospath] [emupath]" command.
 */
static void loadf_handler()
{
	if (argc != 3) {
		printf("Usage: loadf [ospath] [emupath]\n");
		return;
	}
	loadf(argv[1], argv[2]);
}

/**
 * Handles the load [FILE] command.
 */
static void load_handler()
{
	if (argc != 2) {
		printf("Usage: load [FILE]\n");
		return;
	}

	int ret = benchmark_init_fs((const char *)argv[1]);
	if (ret < 0)
		printf("Benchmark failed: %s.\n", fs_strerror(ret));
}

/**
 * Handles the benchmark [FILE] command.
 */
static void benchmark_handler()
{
	if (argc != 2 && argc != 3) {
		printf("Usage: benchmark [FILE] [repcount]\n");
		return;
	}

	int repcount = 1;
	if (argc == 3)
		repcount = atoi(argv[2]);
	benchmark_lookup((const char *)argv[1], repcount);
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
		ls(NULL);  // This will become cwd
	} else {
		for (int i = 1; i < argc; i++) {
			if (ls(argv[i]) < 0) {
				fs_pstrerror(-ENOFOUND, "ls");
			}
		}
	}
}

/**
 * Handles istat [path] command.
 */
static void istat_handler()
{
	if (argc != 2) {
		printf("Usage: istat [pathname]\n");
		return;
	}

	filestat(argv[1]);
}

/**
 * Process a list of arguments broken down by process()
 */
static int process_args()
{
	// Handle "user programs" such as ls
	if (strcmp(argv[0], "ls") == 0) {
		ls_handler();
		return 0;
	} else if (strcmp(argv[0], "cat") == 0) {
		cat_handler();
		return 0;
	} else if (strcmp(argv[0], "load") == 0) {
		load_handler();
		return 0;
	} else if (strcmp(argv[0], "benchmark") == 0) {
		benchmark_handler();
		return 0;
	} else if (strcmp(argv[0], "show_inline") == 0) {
		show_inline_handler();
		return 0;
	} else if (strcmp(argv[0], "show_regular") == 0) {
		show_regular_handler();
		return 0;
	} else if (strcmp(argv[0], "dirhash_dump") == 0) {
		dirhash_dump_handler();
		return 0;
	} else if (strcmp(argv[0], "readl") == 0) {
		readl_handler();
		return 0;
	} else if (strcmp(argv[0], "loadf") == 0) {
		loadf_handler();
		return 0;
	} else if (strcmp(argv[0], "istat") == 0) {
		istat_handler();
		return 0;
	} else if (strcmp(argv[0], "cd") == 0) {
		cd_handler();
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
void sh(FILE *fp)
{
	char *line = NULL;
	size_t len = 0;

	print_prompt();
	while (getline(&line, &len, fp) != -1) {
		if (fp != stdin)
			printf("%s", line);
		if (process(line) < 0)
			break;
		print_prompt();
	}

	free(line);
}
