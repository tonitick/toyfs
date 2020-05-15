#include "struct.h"

int main() {
    superblock.size_ibmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
    superblock.size_dbmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
    superblock.size_inode = 32; // 32 bytes
    superblock.size_filename = 12; // 12 byes
    superblock.root_inum = 0;
    superblock.num_disk_ptrs_per_inode = 4;

    printf("SIZE_IBMAP = %d\n", SIZE_IBMAP);
    printf("SIZE_DBMAP = %d\n", SIZE_DBMAP);
    printf("SIZE_INODE = %d\n", SIZE_INODE);
    printf("SIZE_FILENAME = %d\n", SIZE_FILENAME);
    printf("ROOT_INUM = %d\n", ROOT_INUM);
    printf("NUM_DISK_PTRS_PER_INODE = %d\n", NUM_DISK_PTRS_PER_INODE);
    printf("NUM_INODE = %d\n", NUM_INODE);
    printf("NUM_DATA_BLKS = %d\n", NUM_DATA_BLKS);
    printf("NUM_BLKS_SUPERBLOCK = %d\n", NUM_BLKS_SUPERBLOCK);
    printf("NUM_BLKS_IMAP = %d\n", NUM_BLKS_IMAP);
    printf("NUM_BLKS_DMAP = %d\n", NUM_BLKS_DMAP);
    printf("NUM_BLKS_INODE_TABLE = %d\n", NUM_BLKS_INODE_TABLE);
    printf("SUPERBLOCK_START_BLK = %d\n", SUPERBLOCK_START_BLK);
    printf("IMAP_START_BLK = %d\n", IMAP_START_BLK);
    printf("DMAP_START_BLK = %d\n", DMAP_START_BLK);
    printf("INODE_TABLE_START_BLK = %d\n", INODE_TABLE_START_BLK);
    printf("DATA_REG_START_BLK = %d\n", DATA_REG_START_BLK);

    printf("SIZE_DATA_BLK_PTR = %d\n", SIZE_DATA_BLK_PTR);
    printf("NUM_PTR_PER_BLK = %d\n", NUM_PTR_PER_BLK);
    printf("NUM_FIRST_LEV_PTR_PER_INODE = %d\n", NUM_FIRST_LEV_PTR_PER_INODE);
    printf("NUM_SECOND_LEV_PTR_PER_INODE = %d\n", NUM_SECOND_LEV_PTR_PER_INODE);
    printf("NUM_THIRD_LEV_PTR_PER_INODE = %d\n", NUM_THIRD_LEV_PTR_PER_INODE);
    printf("NUM_FIRST_TWO_LEV_PTR_PER_INODE = %d\n", NUM_FIRST_TWO_LEV_PTR_PER_INODE);
    printf("NUM_ALL_LEV_PTR_PER_INODE = %d\n", NUM_ALL_LEV_PTR_PER_INODE);


    return 0;
}