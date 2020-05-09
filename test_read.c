#include "my_io.h"
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main() {
    int fd, ret;
    unsigned char* buf;
    ret = posix_memalign((void**)&buf, 512, 512);
    fd = open("/dev/sdb1", O_RDONLY | O_DIRECT);
    if (fd < 0) {
        perror("open /dev/sdb1 failed\n");
        exit(1);
    }
    
    int block_idx;
    scanf("%d", &block_idx);
    printf("read block %d\n", block_idx);
    io_read(fd, buf, block_idx);
    buf[511] = 0;
    printf("%s\n", buf);
    free(buf);

    return 0;
}