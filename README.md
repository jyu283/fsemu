# FileSystem EMUlator (fsemu)

A user space file system emulator designed to explore the CPU cache performance of different filesystem implementations.

#### Currently under construction.

For now this will remain a very basic file system.

User space and kernel space separation is simulated but not enforced. The emulator's user interface (user space) can only create/modify the file system (kernel space) via a set of "system calls" exposed to it. Of course this can be circumnavigated by simply including a header file in fsemu.c, but where's the fun in that?

Copyright © 2020 Arpaci-Dusseau Systems Lab.
