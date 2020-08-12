"""
fsemu/tools/stracegen.py

Coypright (c) 2020 Jerry Yu - All rights reserved.

Program to process a Linux system call trace (strace) file into
a series of instructions that fsemu can understand and execute.
Used for benchmarking real-world compiling workloads such as
compiling the Linux kernel.

"""

import argparse
import sys
import string

# List of system calls of interest
# [For now we are only interested in system calls that perform
# path name lookups.]
sys_calls = [
	"execve", "access", "openat", "readlink", "stat", "getdents64", 
	"mkdir", "lstat", 
]


#
# Trim a line in the raw strace file
def process_line(line):
	line = line.replace("(", " ").replace(")", " ").replace(",", "")
	return line

#
# IMPORTANT: strace must be generated with the -f flag, which records
# the PID of the process that generated the system call. This function
# will attempt to skip the first space-separated component in the given
# line, and extract the next component as the system call name.
#
def parse_strace(strace):
	line = strace.readline()
	while line:
		instr = process_line(line).split()
		if instr[1] in sys_calls:
			print(instr)
			print(line)
		line = strace.readline()


def strace_gen(in_file, out_file=None):
	# Open input file and re-route output to a file if specified
	saved_stdout = None
	try:
		strace_file = open(in_file, "r")
		if out_file:
			saved_stdout = sys.stdout
			sys.stdout = open(out_file, "w")
	except IOError as err:
		print(err)
		sys.exit(1)

	parse_strace(strace_file)

	if saved_stdout:
		sys.stdout.close()
		sys.stdout = saved_stdout


def main():
	parser = argparse.ArgumentParser(prog="stracegen")
	parser.add_argument(
		"input_file",
		help="strace's output file",
		metavar="FILE"
	)
	parser.add_argument(
		"-o", "--out",
		help="the file to write results to",
		metavar="FILE"
	)

	args = parser.parse_args()
	strace_gen(args.input_file, args.out)


if __name__ == "__main__":
	main()