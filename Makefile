COMPILER = gcc
FILESYSTEM_FILES = toyfs.c

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -Wall -o toyfs `pkg-config fuse --cflags --libs`
	echo 'To Mount: ./toyfs -f [mount point]'

