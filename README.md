# ToyFS

This is a course project of [CUHK CSCI5550 (Spring 2020)](http://www.cse.cuhk.edu.hk/~mcyang/csci5550/2020S/csci5550.html).

ToyFS manages the basic structures of a Unix File System (superblock, inode-bitmap, datablock-bitmap, inodes and datablocks) in block devices (e.g., a USB drive). ToyFS uses Linux direct I/O to bypass the page cache in Linux kernel, and maintains a block cache in user space.

## Dependencies

ToyFS is based on [FUSE](https://libfuse.github.io/). Install dependencies by `$ apt install fuse libfuse-dev`

## Superblock Settings

1. superblock.size_ibmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
2. superblock.size_dbmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
3. superblock.size_inode = 32; // 32 bytes
4. superblock.size_filename = 12; // size of filename
5. superblock.root_inum = 0; // root directory inode number
6. superblock.num_disk_ptrs_per_inode = 4; // number of data block pointers per inode

## Run

1. Hard code block device file path to my_io.h, e.g., `const char* device_path = "/dev/sdb1";`
2. Plug in block device and change the mode of block device file to 777, e.g., `$ sudo chmod 777 /dev/sdb1`
3. `$ cd /path/to/toyfs`
4. `$ make`
3. Create a directory for mounting toyfs, e.g.: `$ mkdir mnt`
4. Mount toyfs on the mounting point, e.g.: `$ ./toyfs -f mnt`
5. Create another shell and cd to toyfs mounting point, e.g.: `$ cd /path/to/toyfs/mnt`

## Functions

Currently ToyFS works well with `cd`, `cp`, `cp -r`, `ls`, `mkdir`, `touch`, `echo "string" >> file`, `cat`, `rmdir`, `rm`, hard link `ln`, soft link `ln -s` 
