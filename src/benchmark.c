/**
 * fsemu/src/benchmark.c
 * 
 * Benchmarks.
 */

#include "fs_syscall.h"
#include "fserror.h"
#include "util.h"
#include "fs.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * Populate the file system based on the input file.
 * The input file will contain a list of pathnames, and 
 * this function will call creat on each pathname.
 * 
 * To ensure the integrity of the benchmark, this function
 * will fail on the unsuccessful creation of any single 
 * pathname in the file.
 * 
 * The input file should adhere to the following format:
 * D /AAA
 * D /BBB
 * F /AAA/aaa
 * F /BBB/aaa
 * ...
 * 
 * The leading D/F in each line indicate if this path represents
 * a file or a directory. The pathnames must be sorted in order of
 * dependency. (e.g. "/dir1/file" should not come before "/dir1".)
 * Component names can be anything (see fs.h for max name length.)
 */
int benchmark_init_fs(const char *input_file)
{
	FILE *fp;
	char *line = NULL, *pathname;
	size_t len = 0;
	int ret;

	printf("Initializing file system based on %s...\n", input_file);
	if (!(fp = fopen(input_file, "r"))) {
		perror("open");
		return -1;
	}
	
	clock_t begin = clock();
	while (getline(&line, &len, fp) != -1) {
		line[strlen(line) - 1] = '\0';  // clear trailing \n
		pathname = line + 2;
		if (line[0] == 'D') {
			// pr_debug("mkdir %s\n", pathname);
			ret = fs_mkdir(pathname);
		} else if (line[0] == 'F') {
			// pr_debug("creat %s\n", pathname);
			ret = fs_creat(pathname);
		} else {
			return -1;
		}

		if (ret < 0) {
			return ret;
		}
	}

	clock_t end = clock();
	double runtime_main = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);
	printf("Done in %.3fms.\n", runtime_main);

	fclose(fp);
	return 0;
}

int benchmark_lookup(const char *input_file, int repcount)
{
	FILE *fp;
	char *line = NULL, *pathname;
	size_t len = 0;
	clock_t begin, end;
	double time;

	if (!(fp = fopen(input_file, "r"))) {
		perror("open");
		return -1;
	}

	begin = clock();
	for (int i = 0; i < repcount; i++) {
		while (getline(&line, &len, fp) != -1) {
			line[strlen(line) - 1] = '\0';  // clear trailing \n
			pathname = line + 2;
			lookup(pathname);
		}
		rewind(fp);
	}

	end = clock();
	time = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);
	printf("Average running time per cycle: %.3fms.\n", time/repcount);

	fclose(fp);
	return 0;
}

/**
 * Benchmark function.
 */
void benchmark(const char *input_file)
{
	printf("\n:: Starting benchmark...\n");

	if (benchmark_init_fs(input_file) < 0) {
		printf("Error: benchmark failed.\n");
		return;
	}

	printf("\n[!] Benchmark done.\n\n");
}