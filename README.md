# ToyFS

This is a course project of [CUHK CSCI5550 (Spring 2020)](http://www.cse.cuhk.edu.hk/~mcyang/csci5550/2020S/csci5550.html).

ToyFS manages the basic structures of a Unix File System (superblock, inode-bitmap, datablock-bitmap, inodes and datablocks) inside the memory.

## Dependencies

ToyFS is based on [FUSE](https://libfuse.github.io/). See `docker/Dockerfile` for details.

## Macros

1. SIZE_IBMAP: number of inode, default 32
2. SIZE_DBMAP: number of data blocks, default 512
3. SIZE_PER_DATA_REGION: size of data block in bytes, default 64
4. SIZE_FILENAME: size of filename, default 12
5. ROOT_INUM: root directory inode number, default 0
6. NUM_DISK_PTRS_PER_INODE: number of data block pointers per inode, default 4

## Run

1. `cd /path/to/toyfs`
2. `make`
3. Create a directory for mounting toyfs, e.g.: `mkdir mnt`
4. Mount toyfs on the mounting point, e.g.: `./toyfs -f mnt`
5. Create another shell and cd to toyfs mounting point, e.g.: `cd /path/to/toyfs/mnt`

## Use Docker Container

1. `cd /path/to/toyfs/docker`
2. `docker build -t toyfs-image:toyfs --no-cache -f Dockerfile .`
3. `docker run -it --privileged --device /dev/fuse --name toyfs toyfs-image:toyfs /bin/bash`

## Functions

Currently ToyFs works well with `cd`, `cp`, `cp -r`, `ls`, `mkdir`, `touch`, `echo "string" >> file`, `cat`, `rmdir`, `rm`, hard link `ln`, soft link `ln -s` 


