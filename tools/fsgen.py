"""
fsemu/tools/fsgen.py

Copyright (c) 2020 Jerry Yu - All rights reserved.

Program to generate a file system (by listing every path) given a set of 
parameters. Used to create file system images for lookup benchmarking.

My second Python program. Probably still sh*te.

"""

import argparse
import sys
import string
import random

# List of possible file extensions
file_extensions = [
	"txt", "doc", "docx", "png", "jpeg", "mp4", "mkv", "avi", "c", "cpp", "h",
	"py", "out", "md", "pdf", "odt", "html", "ppt", "pptx", "xls", "xlsx", 
	"bmp", "class", "java", "exe", "mov", "psd", "tar", "rar", "tif", "wav"
]

# It pains me immensely to see inode and dentry with capitalised I's and D's,
# but such is the convention in Python, and standards must be adhered to.
# We will ignore the dentry structure and put the name directly inside the 
# inode, since this is a simple program and we don't care about links.
class Inode():
	avg_nsubdir = 2
	avg_dirsize = 10 
	avg_depth = 5 
	avg_namelen = 10 
	total_inodes = 0

	def __init__(self, name, level, isdir=False):
		self.data = None  # List of contents if directory, None if file
		self.name = name
		self.level = level
		self.isdir = isdir

	def new_entry(self, name, isdir=False):
		ent = Inode(name, self.level + 1, isdir)
		self.data.append(ent)
		Inode.total_inodes += 1
		return ent

	@staticmethod
	def rand_name(namelen):
		alphanumeric = string.ascii_letters + string.digits
		return "".join((random.choice(alphanumeric) for _ in range(namelen)))

	# Populate a directory inode
	def populate(self):
		self.data = []
		self.isdir = True

		# Create a number of regular files
		for _ in range(Inode.avg_dirsize - Inode.avg_nsubdir):
			name = self.rand_name(random.randrange(Inode.avg_namelen - 4, 
												Inode.avg_namelen))
			# apply file extension
			name += "."
			name += random.choice(file_extensions)
			self.new_entry(name)

		if self.level >= Inode.avg_depth:
			return

		# create a number (self.NSUBDIRS) of sub-directories
		for _ in range(Inode.avg_nsubdir):
			name = self.rand_name(random.randrange(Inode.avg_namelen - 2,
													Inode.avg_namelen + 2))
			name += "/"
			dir = self.new_entry(name, isdir=True)
			dir.populate()

		# Shuffle the directory so that directories won't always
		# be the last few dentries.
		random.shuffle(self.data)

	def print(self, prefix=""):
		if not self.isdir:
			return

		prefix += self.name

		if self.data is None:
			return
		subdirs = []
		for ent in self.data:
			if ent.isdir:
				subdirs.append(ent)
			print(prefix + ent.name)
		for ent in subdirs:
			ent.print(prefix)


root = None

def fsgen(ofpath):
	stdout = None 
	if ofpath is not None:
		stdout = sys.stdout
		sys.stdout = open(ofpath, "w")
	root = Inode("/", 0) 
	root.populate()
	root.print()

	# Restore stdout
	if stdout is not None:
		sys.stdout = stdout
	print("Total inodes:", Inode.total_inodes)


def main():
	parser = argparse.ArgumentParser(prog="fsgen")
	parser.add_argument(
		"-d", "--dirsize",
		help="the average directory size",
		type=int,
		default=Inode.avg_dirsize,
		metavar="SIZE")
	parser.add_argument(
		"-t", "--treedepth",
		help="the average depth of the file system tree",
		type=int,
		default=Inode.avg_depth,
		metavar="DEPTH")
	parser.add_argument(
		"-s", "--subdir",
		help="the number of subdirectories per directory",
		type=int,
		default=Inode.avg_nsubdir,
		metavar="NUM"
	)
	parser.add_argument(
		"-n", "--namelen",
		help="the average length of directory and file names",
		type=int,
		default=Inode.avg_namelen,
		metavar="LEN")
	parser.add_argument(
		"-o",
		help="the file to write results to",
		metavar="FILE")
	args = parser.parse_args()

	if args.dirsize < Inode.avg_nsubdir:
		print("Error: dirsize must be greater than", Inode.avg_nsubdir)
		return

	Inode.avg_dirsize = args.dirsize
	Inode.avg_depth = args.treedepth
	Inode.avg_namelen = args.namelen

	fsgen(args.o)
	return

if __name__ == "__main__":
	main()
