/**
 * fsemu/src/sh.c
 * 
 * An interactive shell. Maybe called bfsh -- Basic Filesystem Shell?
 * Pronounced beef shell... or buff shell? Oh decisions decisions.
 */

#include "fsemu.h"
#include "fs_syscall.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAXARGS		16	// probably won't ever need this many
#define PROMPTLEN	strlen(prompt)

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
	write(STDOUT_FILENO, prompt, PROMPTLEN);
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

	pr_debug("arg list (argc=%d): \n", argc);
	for (int i = 0; i < MAXARGS; i++) {
		pr_debug("arg #%d: %s\n", i, argv[i]);
	}

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
