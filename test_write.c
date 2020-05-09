#include "my_io.h"
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main() {
    int fd, ret;
    unsigned char* buf;
    ret = posix_memalign((void**)&buf, 512, 512);
    fd = open("/dev/sdb1", O_WRONLY | O_DIRECT);
    if (fd < 0) {
        perror("open /dev/sdb1 failed\n");
        exit(1);
    }

    for (int i = 0; i < 511; i++) {
        buf[i] = i % 26 + 'a';
    }
    int block_idx;
    scanf("%d", &block_idx);
    printf("write block %d\n", block_idx);
    io_write(fd, buf, block_idx);
    free(buf);

    return 0;
}
