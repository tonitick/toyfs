#include "struct.h"

int main() {
    superblock.size_ibmap = 384; // 4 pages = 384 blocks
    superblock.size_dbmap = 384; // 4 pages = 384 blocks
    superblock.size_inode = 32; // 32 bytes
    superblock.size_filename = 12; // 12 byes
    superblock.root_inum = 0;
    superblock.num_disk_ptrs_per_inode = 4;

    printf("SIZE_IBMAP = %d\n", SIZE_IBMAP);
    printf("SIZE_DBMAP = %d\n", SIZE_DBMAP);

    return 0;
}