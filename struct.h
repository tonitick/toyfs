/* 
Authors:
Zheng Zhong

Data structures and utility functions for toyfs file system 
*/
#include "cache.h"
struct SuperBlock {
    unsigned int size_ibmap;
    unsigned int size_dbmap;
    unsigned int size_inode;
    unsigned int size_filename;
    unsigned int root_inum;
    unsigned int num_disk_ptrs_per_inode;
};
struct SuperBlock superblock;

#define SIZE_IBMAP superblock.size_ibmap
#define SIZE_DBMAP superblock.size_dbmap

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