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

def do_wlgen(in_file, depth):
	line = in_file.readline()
	while line:
		break	# TODO
	

def wlgen(args):
	depth = args.depth
	ifpath = args.input_file
	in_file = open(ifpath, "r")

	stdout = None
	if args.out is not None:
		stdout = sys.stdout
		sys.stdout = open(args.out, "w")

	do_wlgen(in_file, depth)

	if stdout is not None:
		sys.stdout.close()
		sys.stdout = stdout
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
		metavar="SIZE"
	)
	args = parser.parse_args()
	wlgen(args)

if __name__ == "__main__":
	main()
