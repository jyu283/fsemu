"""
fsemu/tools/wlgen.py

Copyright (c) 2020 Jerry Yu - All rights reserved.

Program to generate a lookup workload (by listing every path) given a set of 
parameters and a source file system (in the form of a list of possible paths). 
Used for lookup benchmarking.

My third Python program. Perhaps ever so slightly less sh*te. 

"""

import argparse
import sys
import string
import random

def do_wlgen(in_file, depth, size):
	candidates = []
	line = in_file.readline()
	count = 0
	while line:
		if (line.startswith("D") and line.count("/") == (depth + 1)
			or line.startswith("F") and line.count("/") == depth):
			candidates.append(line)
		line = in_file.readline()

	random.shuffle(candidates)
	# Huh, interesting syntax for a ternary expression
	for i in range(size) if size <= len(candidates) else len(candidates):
		print(candidates[i], end="")
		count += 1
	return count
	

def wlgen(args):
	ifpath = args.input_file
	in_file = open(ifpath, "r")

	stdout = None
	if args.out is not None:
		stdout = sys.stdout
		sys.stdout = open(args.out, "w")

	count = do_wlgen(in_file, args.depth, args.size)

	if stdout is not None:
		sys.stdout.close()
		sys.stdout = stdout
	print("Paths generated:", count)
	in_file.close()
	return


def main():
	parser = argparse.ArgumentParser(prog="wlgen")
	parser.add_argument(
		"-d", "--depth",
		help="the average lookup depth",
		type=int,
		metavar="DEPTH"
	)
	parser.add_argument(
		"input_file",
		help="the file system description",
		metavar="infile"
	)
	parser.add_argument(
		"-o", "--out",
		help="the file to write results to",
		metavar="FILE"
	)
	parser.add_argument(
		"-s", "--size",
		help="workload size",
		type=int,
		metavar="SIZE"
	)
	args = parser.parse_args()
	wlgen(args)

if __name__ == "__main__":
	main()
