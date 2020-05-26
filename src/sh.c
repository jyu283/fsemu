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
#define SYSCALL(x)	if(strcmp(name, #x)) return SYS_ ## x

static const char *prompt = "(fsemu) ";

/**
 * Prints out a pretentious preamble.
 */ 
static inline void print_title(void)
{
	puts("FSEMU (File System EMUlator) 0.01-05.11.20");
	puts("Copyright (C) 2020 Arpaci-Dusseau Systems Lab");
	puts("License GPLv3+: GNU GPL version 3 or later");
	puts("Type \"help\" for help. Type \"quit\" to quit.");
	puts("");
}

/**
 * Prints prompt string to stdout.
 */
static inline void print_prompt(void)
{
	fflush(stdout);
	write(STDOUT_FILENO, prompt, PROMPTLEN);
}

/**
 * Return a function ID for the function named in the string.
 * 
 * WARNING: DO *NOT* change the parameter name ("name"),
 * the SYSCALL(x) macro depends on it! And vice versa do not
 * change the macro.
 */
static int lookup_func(char *name)
{
	SYSCALL(open);
	SYSCALL(close);
	SYSCALL(unlink);
	SYSCALL(link);
	SYSCALL(mkdir);
	SYSCALL(rmdir);
	SYSCALL(creat);
	SYSCALL(lseek);
	SYSCALL(read);
	SYSCALL(write);

	return -1;
}

/**
 * Handles cat command.
 */
static void cat_handler(int argc, char *argv[])
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
static void ls_handler(int argc, char *argv[])
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
static int process_args(int argc, char *argv[])
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
	int func_id = lookup_func(argv[0]);
	if (func_id < 0) {
		printf("Invalid command.\n");
		return 0;
	}

	return 0;
}

/**
 * Process one line of user input.
 * 
 * Returns -1 when quit command is issued by user.
 */
int process(char *line)
{
	int argc = 0;
	char *argv[MAXARGS] = { };

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

	print_title();

	print_prompt();
	while (getline(&line, &len, stdin) != -1) {
		if (process(line) < 0)
			break;
		print_prompt();
	}

	free(line);
}
