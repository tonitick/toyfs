/* 
Authors:
Zheng Zhong

Data structures and utilities for accessing toyfs
*/
#ifndef __UTIL_H_
#define __UTIL_H_

#include "cache.h"
#include <string.h>

unsigned int num_read_requests_without_cache = 0;
unsigned int num_write_requests_without_cache = 0;

#define SIZE_BLOCK 512 // 512 bytes per block

const char* magic_string = "zz_toyfs"; // magic string to identify toyfs

struct SuperBlock {
    unsigned int size_ibmap;
    unsigned int size_dbmap;
    unsigned int size_inode;
    unsigned int size_filename;
    unsigned int root_inum;
    unsigned int num_disk_ptrs_per_inode;
} superblock;

#define SIZE_IBMAP ((int)superblock.size_ibmap)
#define SIZE_DBMAP ((int)superblock.size_dbmap)
#define SIZE_INODE ((int)superblock.size_inode)
#define SIZE_FILENAME ((int)superblock.size_filename)
#define ROOT_INUM ((int)superblock.root_inum)
#define NUM_DISK_PTRS_PER_INODE ((int)superblock.num_disk_ptrs_per_inode)

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

int initialize_block(int block_id) {
    pthread_mutex_lock(&cache_lock);

    struct CacheNode* block_cache = get_block_cache(queue, hash, block_id);
    if (block_cache == NULL) return -1;
    memset(block_cache->block_ptr, 0, SIZE_BLOCK);

    block_cache->dirty = true;

    num_write_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return 0;
}

int set_imap_bit(int ino_num, int bit) {
    pthread_mutex_lock(&cache_lock);
    
    int block_id = IMAP_START_BLK + ino_num / (SIZE_BLOCK * 8);
    int byte_offset = (ino_num % (SIZE_BLOCK * 8)) / 8;
    int bit_offset = (ino_num % (SIZE_BLOCK * 8)) % 8;
    struct CacheNode* imap_cache = get_block_cache(queue, hash, block_id);
    if (imap_cache == NULL) return -1;
    char byte_mask = 1 << bit_offset;
    char byte;
    memcpy(&byte, imap_cache->block_ptr + byte_offset, sizeof(byte));
    if (bit) byte = byte | byte_mask;
    else byte = byte & (~byte_mask);
    memcpy(imap_cache->block_ptr + byte_offset, &byte, sizeof(byte));

    imap_cache->dirty = true;

    num_write_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return 0;
}

int get_imap_bit(int ino_num) {
    pthread_mutex_lock(&cache_lock);

    int block_id = IMAP_START_BLK + ino_num / (SIZE_BLOCK * 8);
    int byte_offset = (ino_num % (SIZE_BLOCK * 8)) / 8;
    int bit_offset = (ino_num % (SIZE_BLOCK * 8)) % 8;
    struct CacheNode* imap_cache = get_block_cache(queue, hash, block_id);
    if (imap_cache == NULL) return -1;
    char byte_mask = 1 << bit_offset;
    char byte;
    memcpy(&byte, imap_cache->block_ptr + byte_offset, sizeof(byte));
    
    num_read_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);
    
    if ((byte & byte_mask) != 0) return 1;
    return 0;
}


int set_dmap_bit(int data_reg_idx, int bit) {
    pthread_mutex_lock(&cache_lock);

    int block_id = DMAP_START_BLK + data_reg_idx / (SIZE_BLOCK * 8);
    int byte_offset = (data_reg_idx % (SIZE_BLOCK * 8)) / 8;
    int bit_offset = (data_reg_idx % (SIZE_BLOCK * 8)) % 8;
    struct CacheNode* dmap_cache = get_block_cache(queue, hash, block_id);
    if (dmap_cache == NULL) return -1;
    char byte_mask = 1 << bit_offset;
    char byte;
    memcpy(&byte, dmap_cache->block_ptr + byte_offset, sizeof(byte));
    if (bit) byte = byte | byte_mask;
    else byte = byte & (~byte_mask);
    memcpy(dmap_cache->block_ptr + byte_offset, &byte, sizeof(byte));

    dmap_cache->dirty = true;

    num_write_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return 0;
}

int get_dmap_bit(int data_reg_idx) {
    pthread_mutex_lock(&cache_lock);

    int block_id = DMAP_START_BLK + data_reg_idx / (SIZE_BLOCK * 8);
    int byte_offset = (data_reg_idx % (SIZE_BLOCK * 8)) / 8;
    int bit_offset = (data_reg_idx % (SIZE_BLOCK * 8)) % 8;
    struct CacheNode* dmap_cache = get_block_cache(queue, hash, block_id);
    if (dmap_cache == NULL) return -1;
    char byte_mask = 1 << bit_offset;
    char byte;
    memcpy(&byte, dmap_cache->block_ptr + byte_offset, sizeof(byte));
    
    num_read_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    if ((byte & byte_mask) != 0) return 1;
    return 0;
}

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
    pthread_mutex_lock(&cache_lock);

    int block_id = INODE_TABLE_START_BLK + (ino_num * SIZE_INODE) / SIZE_BLOCK;
    int inode_offset = (ino_num * SIZE_INODE) % SIZE_BLOCK;
    struct CacheNode* inode_cache = get_block_cache(queue, hash, block_id);
    if (inode_cache == NULL) return -1;
    memcpy(inode_cache->block_ptr + inode_offset + data_offset * sizeof(inode_data), &inode_data, sizeof(inode_data));
    
    inode_cache->dirty = true;

    num_write_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return 0;
}

int get_inode_data(int ino_num, int data_offset) {
    pthread_mutex_lock(&cache_lock);

    int block_id = INODE_TABLE_START_BLK + (ino_num * SIZE_INODE) / SIZE_BLOCK;
    int inode_offset = (ino_num * SIZE_INODE) % SIZE_BLOCK;
    struct CacheNode* inode_cache = get_block_cache(queue, hash, block_id);
    if (inode_cache == NULL) return -1;
    int inode_data = -1;
    memcpy(&inode_data, inode_cache->block_ptr + inode_offset + data_offset * sizeof(inode_data), sizeof(inode_data));

    num_read_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return inode_data;
}

int set_data_block_data(int data_reg_idx, const char* buffer, int size, int offset) {
    pthread_mutex_lock(&cache_lock);

    int block_id = DATA_REG_START_BLK + data_reg_idx;
    struct CacheNode* data_block_cache = get_block_cache(queue, hash, block_id);
    if (data_block_cache == NULL) return -1;
    memcpy(data_block_cache->block_ptr + offset, buffer, size);

    data_block_cache->dirty = true;

    num_write_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return size;
}

int get_data_block_data(int data_reg_idx, char* buffer, int size, int offset) {
    pthread_mutex_lock(&cache_lock);

    int block_id = DATA_REG_START_BLK + data_reg_idx;
    struct CacheNode* data_block_cache = get_block_cache(queue, hash, block_id);
    if (data_block_cache == NULL) return -1;
    memcpy(buffer, data_block_cache->block_ptr + offset, size);

    num_read_requests_without_cache++;
    pthread_mutex_unlock(&cache_lock);

    return size;
}

int initialize_toyfs() {    
    superblock.size_ibmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
    superblock.size_dbmap = 196608; // 4 pages = 384 blocks = 196608 bytes = 1572864 bits
    superblock.size_inode = 32; // 32 bytes
    superblock.size_filename = 12; // 12 byes
    superblock.root_inum = 0;
    superblock.num_disk_ptrs_per_inode = 4;

    pthread_mutex_lock(&cache_lock);
    
    int block_id = SUPERBLOCK_START_BLK;
    struct CacheNode* superblock_cache = get_block_cache(queue, hash, block_id);
    if (superblock_cache == NULL) return -1;

    // initialize superblock
    int magic_str_len = strlen(magic_string); // magic string len toyfs
    memcpy(superblock_cache->block_ptr, magic_string, magic_str_len);
    memcpy(superblock_cache->block_ptr + magic_str_len, &superblock.size_ibmap, sizeof(unsigned int));
    memcpy(superblock_cache->block_ptr + magic_str_len + sizeof(unsigned int), &superblock.size_dbmap, sizeof(unsigned int));
    memcpy(superblock_cache->block_ptr + magic_str_len + 2 * sizeof(unsigned int), &superblock.size_inode, sizeof(unsigned int));
    memcpy(superblock_cache->block_ptr + magic_str_len + 3 * sizeof(unsigned int), &superblock.size_filename, sizeof(unsigned int));
    memcpy(superblock_cache->block_ptr + magic_str_len + 4 * sizeof(unsigned int), &superblock.root_inum, sizeof(unsigned int));
    memcpy(superblock_cache->block_ptr + magic_str_len + 5 * sizeof(unsigned int), &superblock.num_disk_ptrs_per_inode, sizeof(unsigned int));
    superblock_cache->dirty = true;

    pthread_mutex_unlock(&cache_lock);

    // initialize bitmap
    for (int i = 0; i < NUM_BLKS_IMAP; i++) {
        int result = initialize_block(IMAP_START_BLK + i);
        if (result < 0) return result;
    }
    for (int i = 0; i < NUM_BLKS_DMAP; i++) {
        int result = initialize_block(DMAP_START_BLK + i);
        if (result < 0) return result;
    }
    pthread_mutex_unlock(&cache_lock);

    // initialize root directory
    // initialize root inode
    int result = set_imap_bit(ROOT_INUM, 1);
    if (result < 0) return -1;
    result = set_inode_data(ROOT_INUM, 1, INODE_FLAG_OFF); // directory
    if (result < 0) return result;
    result = set_inode_data(ROOT_INUM, 0, INODE_NUM_BLKS_OFF);
    if (result < 0) return result;
    result = set_inode_data(ROOT_INUM, 2, INODE_LINKS_COUNT_OFF);  // direcotry has another "." file pointing to itself
    if (result < 0) return result;
    result = set_inode_data(ROOT_INUM, 0, INODE_USED_SIZE_OFF);
    if (result < 0) return result;

    return 0;
}

int get_superblock() {
    pthread_mutex_lock(&cache_lock);

    int block_id = SUPERBLOCK_START_BLK;
    struct CacheNode* superblock_cache = get_block_cache(queue, hash, block_id);
    if (superblock_cache == NULL) return -1;

    int magic_str_len = strlen(magic_string); // magic string len toyfs
    char* magic_str_disk = (char*) malloc(magic_str_len + 1);
    magic_str_disk[magic_str_len] = 0;
    memcpy(magic_str_disk, superblock_cache->block_ptr, magic_str_len);
    if (strcmp(magic_str_disk, magic_string) == 0) {
        printf("[TOYFS] toyfs recognized\n");
        memcpy(&superblock.size_ibmap, superblock_cache->block_ptr + magic_str_len, sizeof(unsigned int));
        memcpy(&superblock.size_dbmap, superblock_cache->block_ptr + magic_str_len + sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&superblock.size_inode, superblock_cache->block_ptr + magic_str_len + 2 * sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&superblock.size_filename, superblock_cache->block_ptr + magic_str_len + 3 * sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&superblock.root_inum, superblock_cache->block_ptr + magic_str_len + 4 * sizeof(unsigned int), sizeof(unsigned int));
        memcpy(&superblock.num_disk_ptrs_per_inode, superblock_cache->block_ptr + magic_str_len + 5 * sizeof(unsigned int), sizeof(unsigned int));

        pthread_mutex_unlock(&cache_lock);
    }
    else {
        pthread_mutex_unlock(&cache_lock);

        printf("[TOYFS] device %s is not of toyfs format. formatting %s ...\n", device_path, device_path);
        int result = initialize_toyfs();
        if (result < 0) return result;
        printf("[TOYFS] formatting done\n");
    }

    return 0;
}

int write_dirty_blocks_back(struct CacheQueue* queue) {
    pthread_mutex_lock(&cache_lock);
    
    struct CacheNode* cache_node = queue->front;
    while (cache_node != NULL) {
        if (cache_node->dirty) {
            int fd = open(device_path, O_WRONLY | O_DIRECT);
            if (fd < 0) return fd;
            io_write(fd, cache_node->block_ptr, cache_node->block_id);
            int result = close(fd);
            if (result < 0) return result;
            
            cache_node->dirty = false;    
        }
        cache_node = cache_node->queue_next;
    }

    pthread_mutex_unlock(&cache_lock);

    return 0;
}

#endif
