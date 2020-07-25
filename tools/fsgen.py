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
    uniform_dirs = True
    total_inodes = 0
    level_count = []

    def __init__(self, name, level, ndirs, isdir=False):
        self.data = None  # List of contents if directory, None if file
        self.name = name
        self.level = level
        self.isdir = isdir
        # If uniform directories is turned off, allow customisation
        if Inode.uniform_dirs is False:
            self.ndirs = ndirs if ndirs > 1 else 1
        else:
            self.ndirs = Inode.avg_nsubdir
        if (len(Inode.level_count)) <= level:
            Inode.level_count.insert(level, 0)
        Inode.level_count[level] += 1

    def new_entry(self, name, isdir=False):
        if Inode.uniform_dirs is True:
            ent = Inode(name, self.level + 1, Inode.avg_nsubdir, isdir)
        else:
            ent = Inode(name, self.level + 1, self.ndirs - 2, isdir)
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

        names = []  # Keep track of the names generated for this directory

        # Create a number of regular files
        for _ in range(Inode.avg_dirsize - self.ndirs):
            while True:
                name = self.rand_name(random.randrange(Inode.avg_namelen - 2, 
                                                Inode.avg_namelen))
                # apply file extension
                name += "."
                name += random.choice(file_extensions)
                if name not in names:
                    names.append(names)
                    self.new_entry(name)
                    break

        if self.level >= Inode.avg_depth:
            return

        # create a number (self.NSUBDIRS) of sub-directories
        names.clear()
        for _ in range(self.ndirs):
            while True:
                name = self.rand_name(random.randrange(Inode.avg_namelen - 2,
                                                        Inode.avg_namelen + 2))
                name += "/"
                if name not in names:
                    names.append(names)
                    dir = self.new_entry(name, isdir=True)
                    dir.populate()
                    break

        # Shuffle the directory so that directories won't always
        # be the last few dentries.
        random.shuffle(self.data)

    def print_dir(self, prefix=""):
        if not self.isdir:
            return
        if self.data is None:
            return

        prefix += self.name
        subdirs = []
        for ent in self.data:
            if ent.isdir:
                subdirs.append(ent)
                print("D " + prefix + ent.name)
            else:
                print("F " + prefix + ent.name)
        for ent in subdirs:
            ent.print_dir(prefix)


root = None

def fsgen(ofpath):
    print("Inode.uniform_dirs:", Inode.uniform_dirs)
    stdout = None 
    if ofpath is not None:
        stdout = sys.stdout
        sys.stdout = open(ofpath, "w")
    if Inode.uniform_dirs is True:
        root = Inode("/", 0, isdir=True, ndirs=Inode.avg_nsubdir) 
    else:
        root = Inode("/", 0, isdir=True, ndirs=Inode.avg_dirsize)
    root.populate()
    root.print_dir()

    # Restore stdout
    if stdout is not None:
        sys.stdout = stdout
    print("Total inodes:", Inode.total_inodes)
    for i in range(len(Inode.level_count)):
        print("Level", i, "contains", Inode.level_count[i], "inode(s).")


def str2bool(v):
    if isinstance(v, bool):
       return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


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
        metavar="NUM")
    parser.add_argument(
        "-n", "--namelen",
        help="the average length of directory and file names",
        type=int,
        default=Inode.avg_namelen,
        metavar="LEN")
    parser.add_argument(
        "-o", "--output",
        help="the file to write results to",
        metavar="FILE")
    parser.add_argument(
        "--uniform",
        help="if FALSE, there will be more subdirectories on top levels; "
             "if TRUE, all directories will have the same number of "
             "subdirectories, as specified by the --subdir flag",
        metavar="<bool>",
        type=str2bool,
        default=True)
    args = parser.parse_args()

    if args.dirsize < args.subdir:
        print("Error: dirsize must be greater than", Inode.avg_nsubdir)
        return

    Inode.avg_dirsize = args.dirsize
    Inode.avg_depth = args.treedepth
    Inode.avg_namelen = args.namelen
    Inode.avg_nsubdir = args.subdir
    Inode.uniform_dirs = args.uniform
    print(args)

    fsgen(args.output)
    return

if __name__ == "__main__":
    main()
