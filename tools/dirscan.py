"""
fsemu/tools/dirscan.py

Copyright (c) 2020 Jerry Yu - All rights reserved.

Program to recursively print out every pathname under a specific directory.
Used to generate realistic file system layouts in the emulator. 

Also my first time writing a python program, so there's probably bad C 
mannerisms everywhere. Even this header comment probably looks more like 
a Linux kernel header comment than a proper Python-styled one.

"""

import argparse
import os, sys


def do_dirscan(prefix, pathname):
	dir = os.scandir(pathname)
	for dent in dir:
		relpath = os.path.relpath(dent.path, prefix)
		if dent.is_dir():
			print("D /" + relpath + "/")
			do_dirscan(prefix, dent.path)
		elif dent.is_file():
			print("F /" + relpath) 


def dirscan(pathname, ofpath):
	if not os.path.exists(pathname):
		print("Error: path does not exist")
		return

	# If specified output file, redirect stdout to file
	if ofpath is not None:
		sys.stdout = open(ofpath, "w")

	# pathname is both prefix and starting position
	do_dirscan(pathname, pathname)
	return


def main():
	parser = argparse.ArgumentParser(prog="dirscan")
	parser.add_argument(
		"path",
		help="the directory to scan",
		default=".")
	parser.add_argument(
		"-o", "--out",
		metavar="FILE",
		help="specify the output file")

	args = parser.parse_args()
	dirscan(args.path, args.out)
	return

if __name__ == "__main__":
	main()