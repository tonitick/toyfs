COMPILER = gcc
FILESYSTEM_FILES = toyfs.c

build: $(FILESYSTEM_FILES)
	$(COMPILER) -D_GNU_SOURCE $(FILESYSTEM_FILES) -Wall -o toyfs `pkg-config fuse --cflags --libs`
	echo 'To Mount: ./toyfs -f [mount point]'

