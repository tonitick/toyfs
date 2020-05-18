/* 
Authors:
Zheng Zhong

Reference:
    [1] fuse getattr: https://libfuse.github.io/doxygen/structfuse__operations.html#a60144dbd1a893008d9112e949300eb77
    [2] stat struct: http://man7.org/linux/man-pages/man2/stat.2.html
    [3] inode struct: http://books.gigatux.nl/mirror/kerneldevelopment/0672327201/ch12lev1sec6.html
    [4] <sys/types.h> header: http://www.doc.ic.ac.uk/~svb/oslab/Minix/usr/include/sys/types.h
*/
#define FUSE_USE_VERSION 29

#define SIZE_DIR_ITEM (SIZE_FILENAME + 4) // size of directory item, 4 bytes for inode number
#define SIZE_DATA_BLK_PTR 4 // size of data block pointers

#define NUM_PTR_PER_BLK (SIZE_BLOCK / SIZE_DATA_BLK_PTR)
#define NUM_FIRST_LEV_PTR_PER_INODE (NUM_DISK_PTRS_PER_INODE - 2)
#define NUM_SECOND_LEV_PTR_PER_INODE NUM_PTR_PER_BLK
#define NUM_THIRD_LEV_PTR_PER_INODE NUM_PTR_PER_BLK * NUM_PTR_PER_BLK
#define NUM_FIRST_TWO_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE)
#define NUM_ALL_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE + NUM_THIRD_LEV_PTR_PER_INODE)

#include "util.h"
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>


int read_block(int ino_num, int blk_idx, char* buffer) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + blk_idx);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;        
        printf("[DBUG INFO] read_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        int result = get_data_block_data(first_level_data_reg_idx, buffer, SIZE_BLOCK, 0);
        if (result < 0) return result;

        return SIZE_BLOCK;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 2);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] read_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        // second level block
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
       
        printf("[DBUG INFO] read_block {second level}: second_level_data_reg_idx = %d\n", second_level_data_reg_idx);
        result = get_data_block_data(second_level_data_reg_idx, buffer, SIZE_BLOCK, 0);
        if (result < 0) return result;

        return SIZE_BLOCK;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 1);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] read_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        // second level block
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] read_block {second level}: second_level_data_reg_idx = %d\n", second_level_data_reg_idx);

        // third level block
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        int third_level_data_reg_idx = -1;
        result = get_data_block_data(second_level_data_reg_idx, (char*) &third_level_data_reg_idx, sizeof(third_level_data_reg_idx), second_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (third_level_data_reg_idx < 0 || third_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] read_block {third level}: data_reg_idx = %d\n", third_level_data_reg_idx);

        result = get_data_block_data(third_level_data_reg_idx, buffer, SIZE_BLOCK, 0);
        if (result < 0) return result;

        return SIZE_BLOCK;
    }

    printf("[ERROR] read_block: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
    return -1;
}

int write_block(int ino_num, int blk_idx, const char* buffer) {    
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;

    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + blk_idx);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        int result = set_data_block_data(first_level_data_reg_idx, buffer, SIZE_BLOCK, 0);
        if (result < 0) return result;

        return SIZE_BLOCK;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 2);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        // second level block
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {second level}: second_level_data_reg_idx = %d\n", second_level_data_reg_idx);
        
        printf("[DBUG INFO] read_block {second level}: second_level_data_reg_idx = %d\n", second_level_data_reg_idx);
        result = set_data_block_data(second_level_data_reg_idx, buffer, SIZE_BLOCK, 0);

        return SIZE_BLOCK;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 1);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {first level}: first_level_data_reg_idx = %d\n", first_level_data_reg_idx);

        // second level block
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {second level}: second_level_data_reg_idx = %d\n", second_level_data_reg_idx);

        // third level block
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        int third_level_data_reg_idx = -1;
        result = get_data_block_data(second_level_data_reg_idx, (char*) &third_level_data_reg_idx, sizeof(third_level_data_reg_idx), second_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (third_level_data_reg_idx < 0 || third_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        printf("[DBUG INFO] write_block {third level}: third_level_data_reg_idx = %d\n", third_level_data_reg_idx);

        result = set_data_block_data(third_level_data_reg_idx, buffer, SIZE_BLOCK, 0);
        if (result < 0) return result;

        return SIZE_BLOCK;
    }

    printf("[DBUG INFO] write_block: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
    return -1;
}

int get_new_inode() {
    static int ino_num = 0;
    for (int i = 0; i < NUM_INODE; i ++) {
        int new_inode = (ino_num + i) % NUM_INODE;
        if (!get_imap_bit(new_inode)) {
            int result = set_imap_bit(new_inode, 1);
            if (result < 0) return result;
            
            ino_num = new_inode;
            
            return ino_num;
        }
    }
    
    printf("[DBUG INFO] get_new_inode: inodes are used up\n");
    return -ENOSPC; // no space left on device [4]
}

int get_new_block() {
    static int block_idx = 0;
    printf("[DBUG INFO] get_new_block: NUM_DATA_BLKS = %d\n", NUM_DATA_BLKS);
    for (int i = 0; i < NUM_DATA_BLKS; i ++) {
        int new_block = (block_idx + i) % NUM_DATA_BLKS;
        if (!get_dmap_bit(new_block)) {
            int result = set_dmap_bit(new_block, 1);
            if (result < 0) return result;
            
            result = initialize_block(DATA_REG_START_BLK + new_block);
            if (result < 0) return result;

            block_idx = new_block;
            
            return block_idx;
        }
    }
    
    printf("[DBUG INFO] get_new_block: blocks are used up\n");
    return -ENOSPC; // no space left on device [4]
}

int assign_block(int ino_num, int blk_idx) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    
    int num_blocks = get_inode_data(ino_num, INODE_NUM_BLKS_OFF);
    if (blk_idx != num_blocks) return -1;
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] assign_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level pointer
        int first_level_data_reg_idx = get_new_block();
        if (first_level_data_reg_idx < 0) return first_level_data_reg_idx;
        int result = set_inode_data(ino_num, first_level_data_reg_idx, INODE_BLK_PTR_OFF + blk_idx);
        if (result < 0) return result;
        printf("[DBUG INFO] assign_block {direct data block}: %d\n", first_level_data_reg_idx);
        
        result = set_inode_data(ino_num, num_blocks + 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;
        
        return 0;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] assign_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level pointer
        int first_level_data_reg_idx = -1;
        if (blk_idx == NUM_FIRST_LEV_PTR_PER_INODE) {
            first_level_data_reg_idx = get_new_block();
            if (first_level_data_reg_idx < 0) return first_level_data_reg_idx;
            int result = set_inode_data(ino_num, first_level_data_reg_idx, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 2);
            if (result < 0) return result;
            printf("[DBUG INFO] assign_block {direct pointer block}: %d\n", first_level_data_reg_idx);
        }
        else {
            first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 2);
            if (first_level_data_reg_idx < 0) return first_level_data_reg_idx;
        }

        // second level pointer
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        int second_level_data_reg_idx = get_new_block();
        if (second_level_data_reg_idx < 0) return second_level_data_reg_idx;
        int result = set_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        printf("[DBUG INFO] assign_block {indirect data block}: %d\n", second_level_data_reg_idx);
        
        result = set_inode_data(ino_num, num_blocks + 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;
        
        return 0;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] assign_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level pointer
        int first_level_data_reg_idx = -1;
        if (blk_idx == NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
            first_level_data_reg_idx = get_new_block();
            if (first_level_data_reg_idx < 0) return first_level_data_reg_idx;
            int result = set_inode_data(ino_num, first_level_data_reg_idx, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 1);
            if (result < 0) return result;
            printf("[DBUG INFO] assign_block {direct pointer block}: %d\n", first_level_data_reg_idx);
        }
        else {
            first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 1);
            if (first_level_data_reg_idx < 0) return first_level_data_reg_idx;
        }

        // second level pointer
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        int second_level_data_reg_idx = -1;
        if (second_level_offset == 0) {
            second_level_data_reg_idx = get_new_block();
            if (second_level_data_reg_idx < 0) return second_level_data_reg_idx;
            int result = set_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
            if (result < 0) return result;
            printf("[DBUG INFO] assign_block {indirect pointer block}: %d\n", second_level_data_reg_idx);
        }
        else {
            int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
            if (result < 0) return result;
        }

        // third level pointer
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        int third_level_data_reg_idx = get_new_block();
        if (third_level_data_reg_idx < 0) return third_level_data_reg_idx;
        int result = set_data_block_data(second_level_data_reg_idx, (char*) &third_level_data_reg_idx, sizeof(third_level_data_reg_idx), second_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;        
        printf("[DBUG INFO] assign_block {double indirect data block}: %d\n", third_level_data_reg_idx);
        
        result = set_inode_data(ino_num, num_blocks + 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;

        return 0;
    }
    // file too large
    if (blk_idx >= NUM_ALL_LEV_PTR_PER_INODE) {
        return -EFBIG; // file too large [4]
    }

    return -1;
}

int reclaim_block(int ino_num, int blk_idx) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;

    int num_blocks = get_inode_data(ino_num, INODE_NUM_BLKS_OFF);
    if (blk_idx != num_blocks - 1) return -1;
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] reclaim_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // get first level block index
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + blk_idx);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        
        // reclaim first level block
        int result = set_dmap_bit(first_level_data_reg_idx, 0);
        if (result < 0) return result;
        printf("[DBUG INFO] reclaim_block {direct data block}: %d\n", first_level_data_reg_idx);

        result = set_inode_data(ino_num, num_blocks - 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;
        
        return 0;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] reclaim_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // get first level block index
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 2);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;

        // get second level block index
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        
        // reclaim second level block
        result = set_dmap_bit(second_level_data_reg_idx, 0);
        if (result < 0) return result;
        printf("[DBUG INFO] reclaim_block {indirect data block}: %d\n", second_level_data_reg_idx);

        // reclaim first level block
        if(blk_idx == NUM_FIRST_LEV_PTR_PER_INODE) {
            int result = set_dmap_bit(first_level_data_reg_idx, 0);
            if (result < 0) return result;
            
            printf("[DBUG INFO] reclaim_block {direct pointer block}: %d\n", first_level_data_reg_idx);
        }
        
        result = set_inode_data(ino_num, num_blocks - 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;
        
        return 0;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] reclaim_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // get first level block index
        int first_level_data_reg_idx = get_inode_data(ino_num, INODE_BLK_PTR_OFF + NUM_DISK_PTRS_PER_INODE - 1);
        if (first_level_data_reg_idx < 0 || first_level_data_reg_idx >= NUM_DATA_BLKS) return -1;

        // get second level block index
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        int second_level_data_reg_idx = -1;
        int result = get_data_block_data(first_level_data_reg_idx, (char*) &second_level_data_reg_idx, sizeof(second_level_data_reg_idx), first_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (second_level_data_reg_idx < 0 || second_level_data_reg_idx >= NUM_DATA_BLKS) return -1;

        // third level block
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        int third_level_data_reg_idx = -1;
        result = get_data_block_data(second_level_data_reg_idx, (char*) &third_level_data_reg_idx, sizeof(third_level_data_reg_idx), second_level_offset * SIZE_DATA_BLK_PTR);
        if (result < 0) return result;
        if (third_level_data_reg_idx < 0 || third_level_data_reg_idx >= NUM_DATA_BLKS) return -1;
        
        // reclaim third level block
        result = set_dmap_bit(third_level_data_reg_idx, 0);
        if (result < 0) return result;
        printf("[DBUG INFO] reclaim_block {double indirect data block}: %d\n", third_level_data_reg_idx);

        // reclaim second level block
        if (second_level_offset == 0) {
            int result = set_dmap_bit(second_level_data_reg_idx, 0);
            if (result < 0) return result;
            printf("[DBUG INFO] reclaim_block {indirect pointer block}: %d\n", second_level_data_reg_idx);
        }

        // reclaim first level block
        if(blk_idx == NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
            int result = set_dmap_bit(first_level_data_reg_idx, 0);
            if (result < 0) return result;
            printf("[DBUG INFO] reclaim_block {direct pointer block}: %d\n", first_level_data_reg_idx);
        }        
        
        result = set_inode_data(ino_num, num_blocks - 1, INODE_NUM_BLKS_OFF);
        if (result < 0) return result;

        return 0;
    }

    return -1;
}

int read_(int ino_num, char* buffer, size_t size, off_t offset) {
    if (offset < 0 || size < 0) return -1;
    if (size == 0) return 0;
    int read_size = 0;
    int cur_offset = offset;
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return file_size;
    char blk_buff[SIZE_BLOCK];
    while (cur_offset < file_size && read_size < size) {
        int blk_idx = cur_offset / SIZE_BLOCK;
        int blk_offset = cur_offset % SIZE_BLOCK;
        int read_bytes = read_block(ino_num, blk_idx, blk_buff);
        if (read_bytes != SIZE_BLOCK) return -1;
        // increment should be the minimun of (read_bytes - blk_offset, file_size - cur_offset, size - read_size)
        int increment = (read_bytes - blk_offset < file_size - cur_offset) ? read_bytes - blk_offset : file_size - cur_offset;
        increment = (increment < size -  read_size) ? increment : size - read_size;
        memcpy(buffer + read_size, blk_buff + blk_offset, increment);
        cur_offset += increment;
        read_size += increment;
    }
    
    return read_size;
}

int write_(int ino_num, const char* buffer, size_t size, off_t offset) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    // add blocks
    int cur_block_num = get_inode_data(ino_num, INODE_NUM_BLKS_OFF);
    if (cur_block_num < 0) return cur_block_num;
    if (offset < 0 || size < 0) return -1;
    if (size == 0) return 0;
    int end_block_num = (offset + size - 1) / SIZE_BLOCK + 1;
    for (int i = cur_block_num; i < end_block_num; i++) {
        int result = assign_block(ino_num, i);
        if (result < 0) return result;
    }
    
    int write_size = 0;
    int cur_offset = offset;
    char blk_buff[SIZE_BLOCK];
    while (write_size < size) {
        int blk_idx = cur_offset / SIZE_BLOCK;
        int blk_offset = cur_offset % SIZE_BLOCK;
        int read_bytes = read_block(ino_num, blk_idx, blk_buff);
        if (read_bytes != SIZE_BLOCK) return -1;
        // increment should be the minimun of (SIZE_BLOCK - blk_offset, size - write_size)
        int increment = SIZE_BLOCK - blk_offset < size - write_size ? SIZE_BLOCK - blk_offset : size - write_size;
        memcpy(blk_buff + blk_offset, buffer + write_size, increment);
        int write_bytes = write_block(ino_num, blk_idx, blk_buff);
        if (write_bytes != SIZE_BLOCK) return -1;
        cur_offset += increment;
        write_size += increment;
    }
    
    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return file_size;
    int result = set_inode_data(ino_num, (file_size > offset + size) ? file_size : offset + size, INODE_USED_SIZE_OFF);
    if (result < 0) return result;
        
    return write_size;
}

int remove_file_blocks(int ino_num) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;

    int cur_block_num = get_inode_data(ino_num, INODE_NUM_BLKS_OFF);
    for (int i = cur_block_num - 1; i >=0; i--) {
        int result = reclaim_block(ino_num, i);
        if (result < 0) return result;
    }
    
    return 0;
}

int find_dir_entry_ino(int ino_num, const char* name) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    if (get_inode_data(ino_num, INODE_FLAG_OFF) != 1 ) return -ENOTDIR; // not a directory [4]
    
    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size % SIZE_DIR_ITEM != 0) return -1;
    char* buffer = (char*) malloc(file_size);
    int read_bytes = read_(ino_num, buffer, file_size, 0);
    if (read_bytes != file_size) return -1;
    
    char filename[SIZE_FILENAME + 1];
    memset(filename, 0, SIZE_FILENAME + 1);
    int offset = 0;
    while (offset < file_size) {
        int sub_ino_num = -1;
        memcpy(&sub_ino_num, buffer + offset, sizeof(sub_ino_num));
        memcpy(filename, buffer + offset + sizeof(sub_ino_num), SIZE_FILENAME);
        if (strcmp(filename, name) == 0 && sub_ino_num >=0) {
            free(buffer);
            return sub_ino_num;
        }
        offset += SIZE_DIR_ITEM;
    }

    free(buffer);
    return -ENOENT; // no such file or directory [4]
}

int remove_dir_entry(int ino_num, const char* name) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;
    if (get_inode_data(ino_num, INODE_FLAG_OFF) != 1) return -ENOTDIR; // not a directory [4]
    
    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size % SIZE_DIR_ITEM != 0) return -1;
    char* buffer = (char*) malloc(file_size);
    int read_bytes = read_(ino_num, buffer, file_size, 0);
    if (read_bytes != file_size) return -1;
    
    char filename[SIZE_FILENAME + 1];
    memset(filename, 0, SIZE_FILENAME + 1);
    int offset = 0;
    while (offset < file_size) {
        int sub_ino_num = -1;
        memcpy(&sub_ino_num, buffer + offset, sizeof(sub_ino_num));
        memcpy(filename, buffer + offset + sizeof(sub_ino_num), SIZE_FILENAME);
        if (strcmp(filename, name) == 0 && sub_ino_num >=0) break;
        offset += SIZE_DIR_ITEM;
    }
    if (offset < file_size) {
        // write last directory entry to offset
        int last_entry_offset = file_size - SIZE_DIR_ITEM;
        write_(ino_num, buffer + last_entry_offset, SIZE_DIR_ITEM, offset);
        memset(buffer + last_entry_offset, 0, SIZE_DIR_ITEM);
        write_(ino_num, buffer + last_entry_offset, SIZE_DIR_ITEM, last_entry_offset);

        // reclaim block if necessary
        file_size = file_size - SIZE_DIR_ITEM;
        int blk_idx = file_size / SIZE_BLOCK;
        if (file_size % SIZE_BLOCK == 0) {
            int result = reclaim_block(ino_num, blk_idx);
            if (result < 0) return result;
        }
        int result = set_inode_data(ino_num, file_size, INODE_USED_SIZE_OFF);
        if (result < 0) return result;

        free(buffer);
        return 0;
    }

    free(buffer);
    return -ENOENT; // no such file or directory [4]
}

int rmdir_(int ino_num) {
    if (ino_num < 0 || ino_num >= NUM_INODE) return -1;

    int file_flag = get_inode_data(ino_num, INODE_FLAG_OFF);
    if (file_flag < 0) return file_flag;
    // regular file or soft link
    if (file_flag == 0 || file_flag == 2) {
        int file_links_count = get_inode_data(ino_num, INODE_LINKS_COUNT_OFF);
        if (file_links_count < 0) return file_links_count;
        if (file_links_count > 1) {
            int result = set_inode_data(ino_num, file_links_count - 1, INODE_LINKS_COUNT_OFF);
            if (result < 0) return result;
        }
        else {
            int result = remove_file_blocks(ino_num);
            if (result < 0) return result;
            
            result = set_imap_bit(ino_num, 0);
            if (result < 0) return result;
        }
        
        return 0;
    }
    // direcotory
    else if (file_flag == 1) {
        int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
        if (file_size < 0 || file_size % SIZE_DIR_ITEM != 0) return -1;
        char* buffer = (char*) malloc(file_size);
        int read_bytes = read_(ino_num, buffer, file_size, 0);
        if (read_bytes != file_size) return -1;
        
        char filename[SIZE_FILENAME + 1];
        memset(filename, 0, SIZE_FILENAME + 1);
        int cur_offset = 0;
        while (cur_offset < file_size) {
            int sub_ino_num = -1;
            memcpy(&sub_ino_num, buffer + cur_offset, sizeof(sub_ino_num));
            memcpy(filename, buffer + cur_offset + sizeof(sub_ino_num), SIZE_FILENAME);
            if (sub_ino_num >= 0 && sub_ino_num < NUM_INODE) {
                int entry_flag = get_inode_data(sub_ino_num, INODE_FLAG_OFF);
                if (entry_flag < 0) return entry_flag;
                int result = rmdir_(sub_ino_num); // remove recursively
                if (result < 0) return result;
                if (entry_flag == 1) { // subdir ".." link
                    int file_links_count = get_inode_data(ino_num, INODE_LINKS_COUNT_OFF);
                    if (file_links_count < 0) return file_links_count;
                    int result = set_inode_data(ino_num, file_links_count - 1, INODE_LINKS_COUNT_OFF);
                    if (result < 0) return result;
                }
            }
            
            int result = remove_dir_entry(ino_num, filename);
            if (result < 0) return result;
            
            cur_offset += SIZE_DIR_ITEM;
        }

        int file_links_count = get_inode_data(ino_num, INODE_LINKS_COUNT_OFF);
        if (file_links_count < 0) return file_links_count;
        if (file_links_count != 2) {
            printf("[ERROR] rmdir_: ino_num = %d, links_count = %d\n", ino_num, file_links_count);
            return -1; // self and "." pointing to self
        }
        int result = remove_file_blocks(ino_num);
        if (result < 0) return result;

        result = set_imap_bit(ino_num, 0);
        if (result < 0) return result;

        return 0;
    }
    else return -1; // not suported type
}

int get_inode_number(const char* path) {
    // get inode number from an absolute path
    int plen = strlen(path);
    int ino_num = ROOT_INUM; // root
    int lpos = 1; // bypass the preceding '/'
    while (lpos < plen) {
        int rpos = lpos;
        while (rpos < plen && path[rpos] != '/')
            rpos++;
        char name[SIZE_FILENAME + 1];
        memset(name, 0, SIZE_FILENAME + 1);
        memcpy(name, path + lpos, rpos - lpos);
        int new_ino_num = find_dir_entry_ino(ino_num, name);
        if (new_ino_num < 0) return new_ino_num;
        ino_num = new_ino_num;
        lpos = rpos + 1;
    }
    return ino_num;
}

static int do_getattr(const char* path, struct stat* st) {
    printf("[DIRECT CALL INFO] getattr: path = %s\n", path);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    if (ino_num >= NUM_INODE) return -1;
    
    // st_dev is ignored [1]
    st->st_ino = ino_num; // set to inode number inside the file system [1]
    st->st_nlink = get_inode_data(ino_num, INODE_LINKS_COUNT_OFF);
    if (st->st_nlink < 0) return -1;
    st->st_uid = getuid(); // currently no owner user id info in inode, set to user who mounts the fs
    st->st_gid = getgid(); // currently no owner group id info in inode, set to user group who mounts the fs
    // st_rdev is ignored?
    st->st_atime = time(NULL); // currently no last access time info in inode, set to current time
    st->st_mtime = time(NULL); // currently no last modify time info in inode, set to current time
    st->st_ctime = time(NULL); // currently no last change time info in inode, set to current time
    // st_blksize is ignored
    st->st_blocks = get_inode_data(ino_num, INODE_NUM_BLKS_OFF); // set to number of data blocks assigned, slightly different from [2] 
    if (st->st_blocks < 0) return -1;
    st->st_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF); // same as [2]
    if (st->st_size < 0) return -1;
    
    int file_flag = get_inode_data(ino_num, INODE_FLAG_OFF);
    if (file_flag < 0) return file_flag;
    if (file_flag == 0) {
        st->st_mode = S_IFREG | 0644; // currently no mode info in inode, set to 644 for regular
    }
    else if (file_flag == 1) {
        st->st_mode = S_IFDIR | 0755; // currently no mode info in inode, set to 755 for directory
    }
    else if (file_flag == 2) {
        st->st_mode = S_IFLNK | 0777; // currently no mode info in inode, set to 777 for soft link
    }

    return 0;
}

static int do_readdir(const char* path, void* res_buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    printf("[DIRECT CALL INFO] readdir: path = %s\n", path);

    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    if (ino_num >= NUM_INODE) return -1;
    
    if (get_inode_data(ino_num, INODE_FLAG_OFF) != 1) return -ENOTDIR; // not a directory [4]

    filler(res_buf, ".", NULL, 0); // current Directory
    filler(res_buf, "..", NULL, 0); // parent Directory

    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0 || file_size % SIZE_DIR_ITEM != 0) return -1;
    char* buffer = (char*) malloc(file_size);
    int read_bytes = read_(ino_num, buffer, file_size, 0);
    if (read_bytes != file_size) return -1;
    
    char filename[SIZE_FILENAME + 1];
    memset(filename, 0, SIZE_FILENAME + 1);
    int cur_offset = 0;
    while (cur_offset < file_size) {
        int sub_ino_num = -1;
        memcpy(&sub_ino_num, buffer + cur_offset, sizeof(sub_ino_num));
        memcpy(filename, buffer + cur_offset + sizeof(sub_ino_num), SIZE_FILENAME);
        if (sub_ino_num >= 0) filler(res_buf, filename, NULL, 0);
        cur_offset += SIZE_DIR_ITEM;
    }

    free(buffer);
    return 0;
}

static int do_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* fi) {
    printf("[DIRECT CALL INFO] read: path = %s, size = %ld, offset = %ld\n", path, size, offset);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    return read_(ino_num, buffer, size, offset);
}

static int do_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* info) {
    printf("[DIRECT CALL INFO] write: path = %s, size = %ld, offset = %ld\n", path, size, offset);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    return write_(ino_num, buffer, size, offset);
}

static int do_mkdir(const char* path, mode_t mode) {
    printf("[DIRECT CALL INFO] mkdir: path = %s\n", path);

    // get new file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0) return -EEXIST; // file exists [4]

    // file info
    file_ino_num = get_new_inode();
    if (file_ino_num < 0) return file_ino_num;
    if (file_ino_num >= NUM_INODE) return -1;
    int result = set_inode_data(file_ino_num, 1, INODE_FLAG_OFF); // directory
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_NUM_BLKS_OFF);
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 2, INODE_LINKS_COUNT_OFF);  // direcotry has another "." file pointing to itself
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_USED_SIZE_OFF);
    if (result < 0) return result;
    
    // parent directory info
    int links_count = get_inode_data(parent_ino_num, INODE_LINKS_COUNT_OFF);
    if (links_count < 0) return links_count;
    result = set_inode_data(parent_ino_num, links_count + 1, INODE_LINKS_COUNT_OFF);
    if (result < 0) return result;
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int file_size = get_inode_data(parent_ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return -1;
    int write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, file_size);
    
    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static int do_mknod(const char* path, mode_t mode, dev_t rdev) {
    printf("[DIRECT CALL INFO] mknod: path = %s\n", path);

    // get new file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0) return -EEXIST; // file exists [4]

    // file info
    file_ino_num = get_new_inode();
    if (file_ino_num < 0) return file_ino_num;
    if (file_ino_num >= NUM_INODE) return -1;
    int result = set_inode_data(file_ino_num, 0, INODE_FLAG_OFF); // regular
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_NUM_BLKS_OFF);
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 1, INODE_LINKS_COUNT_OFF);
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_USED_SIZE_OFF);
    if (result < 0) return result;
    
    // parent directory info
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int file_size = get_inode_data(parent_ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return -1;
    int write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, file_size);

    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static int do_unlink(const char* path) {
    printf("[DIRECT CALL INFO] unlink: path = %s\n", path);

    // get delete file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num < 0) return file_ino_num;
    if (file_ino_num >= NUM_INODE) return -1;
    
    // remove file
    if (get_inode_data(file_ino_num, INODE_FLAG_OFF) == 1) return -EISDIR; // is a directory [4]
    int links_count = get_inode_data(file_ino_num, INODE_LINKS_COUNT_OFF);
    if (links_count < 0) return links_count;
    if (links_count < 1) return -1;
    if (links_count > 1) {
        int result = set_inode_data(file_ino_num, links_count - 1, INODE_LINKS_COUNT_OFF);
        if (result < 0) return result;
    }
    else {
        int result = remove_file_blocks(file_ino_num);
        if (result < 0) return result;
        // free inode
        result = set_imap_bit(file_ino_num, 0);
        if (result < 0) return result;
    }

    // remove parent directory entry
    int result = remove_dir_entry(parent_ino_num, file_name);
    if (result < 0) return result;

    return 0;
}

static int do_rmdir(const char* path) {
    printf("[DIRECT CALL INFO] rmdir: path = %s\n", path);

    // get delete file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num < 0) return file_ino_num;
    if (file_ino_num >= NUM_INODE) return -1;

    // remove directory
    if (get_inode_data(file_ino_num, INODE_FLAG_OFF) != 1) return -ENOTDIR; // not a directory [4]
    int result = rmdir_(file_ino_num);
    if (result < 0) return result;

    // remove parent directory entry
    result = remove_dir_entry(parent_ino_num, file_name);
    if (result < 0) return result;
    int links_count = get_inode_data(parent_ino_num, INODE_LINKS_COUNT_OFF);
    if (links_count < 0) return links_count;
    result = set_inode_data(parent_ino_num, links_count - 1, INODE_LINKS_COUNT_OFF); // the ".." in subdir
    if (result < 0) return result;

    return 0;
}

static int do_link(const char* target_path, const char* path) {
    printf("[DIRECT CALL INFO] link: target_path = %s, path = %s\n", target_path, path);

    // target file info
    int target_ino_num = get_inode_number(target_path);
    if (target_ino_num < 0) return target_ino_num;
    if (target_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(target_ino_num, INODE_FLAG_OFF) == 1) return -EPERM; // operation not permitted [4]: cannot hard link to directory

    // get new file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0 && file_ino_num < NUM_INODE) return -EEXIST; // file exists [4]
    if (file_ino_num >= NUM_INODE) return -1;
    file_ino_num = target_ino_num; // link to target inode

    // file info
    int links_count = get_inode_data(file_ino_num, INODE_LINKS_COUNT_OFF);
    if (links_count < 0) return links_count;
    int result = set_inode_data(file_ino_num, links_count + 1, INODE_LINKS_COUNT_OFF);
    if (result < 0) return result;    

    // parent directory info
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int file_size = get_inode_data(parent_ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return -1;
    int write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, file_size);
    
    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static int do_symlink(const char* target_path, const char* path) {
    printf("[DIRECT CALL INFO] symlink: target_path = %s, path = %s\n", target_path, path);

    // get new file and parent info
    int plen = strlen(path);
    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len <= 0) return -ENOENT; // no such file or directory [4]
    if (file_name_len > SIZE_FILENAME) return -ENAMETOOLONG; // file name too long [4]
    char file_name[SIZE_FILENAME + 1];
    memset(file_name, 0, SIZE_FILENAME + 1);
    memcpy(file_name, path + pos + 1, file_name_len);
    if (pos == 0) pos = 1; // root path 
    char* parent_name = (char*) malloc(pos + 1);
    memset(parent_name, 0, pos + 1);
    memcpy(parent_name, path, pos);

    int parent_ino_num = get_inode_number(parent_name);
    free(parent_name);
    if (parent_ino_num < 0) return parent_ino_num;
    if (parent_ino_num >= NUM_INODE) return -1;
    if (get_inode_data(parent_ino_num, INODE_FLAG_OFF) != 1) return -1; // parent need to be a directory
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0) return -EEXIST; // file exists [4]

    // file info
    file_ino_num = get_new_inode();
    if (file_ino_num < 0) return file_ino_num;
    if (file_ino_num >= NUM_INODE) return -1;
    int result = set_inode_data(file_ino_num, 2, INODE_FLAG_OFF); // soft link
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_NUM_BLKS_OFF);
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 1, INODE_LINKS_COUNT_OFF);
    if (result < 0) return result;
    result = set_inode_data(file_ino_num, 0, INODE_USED_SIZE_OFF);
    if (result < 0) return result;
    int write_bytes = write_(file_ino_num, target_path, strlen(target_path), 0);
    if (write_bytes != strlen(target_path)) return -1;
    
    // parent directory info
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int file_size = get_inode_data(parent_ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return -1;
    write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, file_size);
    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static int do_readlink(const char* path, char* res_buf, size_t buf_len) {
    printf("[DIRECT CALL INFO] readlink: path = %s, buf_len = %lu\n", path, buf_len);

    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    if (ino_num >= NUM_INODE) return -1;
    
    if (get_inode_data(ino_num, INODE_FLAG_OFF) != 2) return -1; // not a link

    memset(res_buf, 0, buf_len);
    int file_size = get_inode_data(ino_num, INODE_USED_SIZE_OFF);
    if (file_size < 0) return file_size;
    int read_size = (file_size < buf_len -1) ? file_size : (buf_len - 1); // buf_len contains a null end for string
    int read_bytes = read_(ino_num, res_buf, read_size, 0);
    if (read_bytes != read_size) return -1;
    
    return 0;
}

static int do_utimens(const char* a, const struct timespec tv[2]) {
    return 0; // an empty function to prevent prompt in 'touch file'
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .read = do_read,
    .write = do_write,
    .mkdir = do_mkdir,
    .mknod = do_mknod,
    .unlink = do_unlink,
    .rmdir = do_rmdir,
    .link = do_link,
    .symlink = do_symlink,
    .readlink = do_readlink,
    .utimens = do_utimens,
};

int main(int argc, char* argv[]) {
    queue = create_cache_queue(1000);
    hash = create_hash_table(1000);

    int result = get_superblock();
    if (result < 0) return -1;

    result = fuse_main(argc, argv, &operations, NULL);
    if (result < 0) return result;

    printf("wirte dirty blocks back ...\n");
    result = write_dirty_blocks_back(queue);
    if (result < 0) return result;

    printf("free cache space ...\n");
    while(!is_queue_empty(queue)) dequeue(queue, hash);
    free(queue);
    free(hash->buckets);
    free(hash);
}
