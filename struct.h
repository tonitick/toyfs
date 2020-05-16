/* 
Authors:
Zheng Zhong

Data structures and utility functions for accessing ToyFS
*/
#include "cache.h"

#define SIZE_BLOCK 512

struct SuperBlock {
    unsigned int size_ibmap;
    unsigned int size_dbmap;
    unsigned int size_inode;
    unsigned int size_filename;
    unsigned int root_inum;
    unsigned int num_disk_ptrs_per_inode;
} superblock;

#define SIZE_IBMAP superblock.size_ibmap
#define SIZE_DBMAP superblock.size_dbmap
#define SIZE_INODE superblock.size_inode
#define SIZE_FILENAME superblock.size_filename
#define ROOT_INUM superblock.root_inum
#define NUM_DISK_PTRS_PER_INODE superblock.num_disk_ptrs_per_inode

#define NUM_INODE (SIZE_IBMAP * 8)
#define NUM_DATA_BLKS (SIZE_DBMAP * 8)
#define NUM_BLKS_SUPERBLOCK 8 // set to 1 page = 8 blocks
#define NUM_BLKS_IMAP (SIZE_IBMAP / SIZE_BLOCK)
#define NUM_BLKS_DMAP (SIZE_DBMAP / SIZE_BLOCK)
#define NUM_BLKS_INODE_TABLE (SIZE_INODE * NUM_INODE / SIZE_BLOCK)

#define SUPERBLOCK_START_BLK 0
#define IMAP_START_BLK NUM_BLKS_SUPERBLOCK
#define DMAP_START_BLK (IMAP_START_BLK + NUM_BLKS_IMAP)
#define INODE_TABLE_START_BLK (DMAP_START_BLK + NUM_BLKS_DMAP)
#define DATA_REG_START_BLK (INODE_TABLE_START_BLK + NUM_BLKS_INODE_TABLE)

#define SIZE_DATA_BLK_PTR 4 // size of data block pointers
#define NUM_PTR_PER_BLK (SIZE_BLOCK / SIZE_DATA_BLK_PTR)
#define NUM_FIRST_LEV_PTR_PER_INODE (NUM_DISK_PTRS_PER_INODE - 2)
#define NUM_SECOND_LEV_PTR_PER_INODE NUM_PTR_PER_BLK
#define NUM_THIRD_LEV_PTR_PER_INODE NUM_PTR_PER_BLK * NUM_PTR_PER_BLK
#define NUM_FIRST_TWO_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE)
#define NUM_ALL_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE + NUM_THIRD_LEV_PTR_PER_INODE)


// inode data offset:
//     0 for flag, 1 for number blocks assigned
//     2 for used size, 3 for links count
//     > 3 for block pointers
#define INODE_FLAG_OFF 0
#define INODE_NUM_BLKS_OFF 1
#define INODE_USED_SIZE_OFF 2
#define INODE_LINKS_COUNT_OFF 3
#define INODE_BLK_PTR_OFF 4

int set_inode_data(int ino_num, int inode_data, int data_offset) {
    int block_id = INODE_TABLE_START_BLK + (ino_num * SIZE_INODE) / SIZE_BLOCK;
    int inode_offset = (ino_num * SIZE_INODE) % SIZE_BLOCK;
    CacheNode* inode_cache = get_block_cache(queue, hash, block_id);
    if (inode_cache == NULL) return -1;
    memcpy(inode_cache->block_ptr + inode_offset + data_offset, &inode_data, sizeof(inode_data));
    inode_cache->dirty = true;

    return 0;
}

int get_inode_flag(int ino_num, int data_offset) {
    int block_id = INODE_TABLE_START_BLK + (ino_num * SIZE_INODE) / SIZE_BLOCK;
    int inode_offset = (ino_num * SIZE_INODE) % SIZE_BLOCK;
    CacheNode* inode_cache = get_block_cache(queue, hash, block_id);
    if (inode_cache == NULL) return -1;
    int inode_data = -1;
    memcpy(&inode_data, inode_cache->block_ptr + inode_offset + data_offset, sizeof(inode_data));

    return inode_data;
}




// bool inode_bitmap[SIZE_IBMAP];
// bool data_bitmap[SIZE_DBMAP];

// struct INode {
//     int flag; // file type {0:REGULAR, 1:DIRECTORY, 2:SOFTLINK}
//     int blocks; // number of blocks used
//     int block[NUM_DISK_PTRS_PER_INODE]; // block pointers
//     int links_count; // number of hard links
//     int size; // file size in byte
// };
// struct INode inode_table[SIZE_IBMAP];

// struct DataBlock {
//     char space[SIZE_PER_DATA_REGION];
// };
// struct DataBlock data_regions[SIZE_DBMAP];