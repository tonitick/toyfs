#include <unistd.h>
#include <assert.h>
#include <stdio.h>

unsigned int num_read_requests = 0;
unsigned int num_write_requests = 0;
size_t block_size = 512; // block size in bytes

void io_read(int fd, void* buf, int index) {
    off_t offset = index * block_size;
    ssize_t read_bytes = pread(fd, buf, block_size, offset);
    assert(read_bytes == block_size);
    num_read_requests++;
}

void io_write(int fd, void* buf, int index) {
    off_t offset = index * block_size;
    ssize_t write_bytes = pwrite(fd, buf, block_size, offset);
    assert(write_bytes == block_size);
    num_write_requests++;
}
