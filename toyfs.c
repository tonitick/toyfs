/* Reference
    [1] fuse getattr: https://libfuse.github.io/doxygen/structfuse__operations.html#a60144dbd1a893008d9112e949300eb77
    [2] stat struct: http://man7.org/linux/man-pages/man2/stat.2.html
    [3] inode struct: http://books.gigatux.nl/mirror/kerneldevelopment/0672327201/ch12lev1sec6.html
    [4] <sys/types.h> header: http://www.doc.ic.ac.uk/~svb/oslab/Minix/usr/include/sys/types.h
*/
#define FUSE_USE_VERSION 30

#define SIZE_IBMAP 32 // number of inode
#define SIZE_DBMAP 512 // number of data blocks
#define SIZE_PER_DATA_REGION 64 // size of data block
#define SIZE_FILENAME 12 // size of filename
#define ROOT_INUM 0 // root directory inode number
#define NUM_DISK_PTRS_PER_INODE 4 // number of data block pointers per inode

#define SIZE_DIR_ITEM (SIZE_FILENAME + 4) // size of directory item, 4 bytes for inode number
#define SIZE_DATA_BLK_PTR 4 // size of data block pointers

#define NUM_PTR_PER_BLK (SIZE_PER_DATA_REGION / SIZE_DATA_BLK_PTR)
#define NUM_FIRST_LEV_PTR_PER_INODE (NUM_DISK_PTRS_PER_INODE - 2)
#define NUM_SECOND_LEV_PTR_PER_INODE NUM_PTR_PER_BLK
#define NUM_THIRD_LEV_PTR_PER_INODE NUM_PTR_PER_BLK * NUM_PTR_PER_BLK
#define NUM_FIRST_TWO_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE)
#define NUM_ALL_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE + NUM_THIRD_LEV_PTR_PER_INODE)

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

// char dir_list[ 256 ][ 256 ];
// int curr_dir_idx = -1;
// 
// char files_list[ 256 ][ 256 ];
// int curr_file_idx = -1;
// 
// char files_content[ 256 ][ 256 ];
// int curr_file_content_idx = -1;

struct SuperBlock {
    unsigned int size_ibmap;
    unsigned int size_dbmap;
    unsigned int size_per_data_region;
    unsigned int size_filename;
    unsigned int root_inum;
    unsigned int num_disk_ptrs_per_inode;
};
struct SuperBlock superblock = {
    .size_ibmap = SIZE_IBMAP,
    .size_dbmap = SIZE_DBMAP,
    .size_per_data_region = SIZE_PER_DATA_REGION,
    .size_filename = SIZE_FILENAME,
    .root_inum = ROOT_INUM,
    .num_disk_ptrs_per_inode = NUM_DISK_PTRS_PER_INODE,
};

// bool inode_bitmap[superblock.size_ibmap];
// bool data_bitmap[superblock.size_dbmap];
bool inode_bitmap[SIZE_IBMAP];
bool data_bitmap[SIZE_DBMAP];

struct INode {
    int flag; // file type {0:REGULAR, 1:DIRECTORY, 2:SOFTLINK}
    int blocks; // number of blocks used
    int block[NUM_DISK_PTRS_PER_INODE]; // block pointers
    int links_count; // number of hard links
    int size; // file size in byte
};
struct INode inode_table[SIZE_IBMAP];

struct DataBlock {
    char space[SIZE_PER_DATA_REGION];
};
struct DataBlock data_regions[SIZE_DBMAP];

int read_block(int ino_num, int blk_idx, char* buffer) {
    struct INode* inode = &inode_table[ino_num];
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int data_reg_idx = inode->block[blk_idx];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {first level}: data_reg_idx = %d\n", data_reg_idx);
        
        memcpy(buffer, first_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        
        // first level block
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 2];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {first level}: data_reg_idx = %d\n", data_reg_idx);

        // second level block
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {second level}: data_reg_idx = %d\n", data_reg_idx);
        
        memcpy(buffer, second_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] read_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        
        // first level block
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 1];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {first level}: data_reg_idx = %d\n", data_reg_idx);

        // second level block
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {second level}: data_reg_idx = %d\n", data_reg_idx);

        // third level block
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        data_reg_idx = (int) second_level_block[second_level_offset * SIZE_DATA_BLK_PTR];
        char* third_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] read_block {third level}: data_reg_idx = %d\n", data_reg_idx);

        memcpy(buffer, third_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }

    printf("[ERROR] read_block: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
    return -1;
}

int write_block(int ino_num, int blk_idx, const char* buffer) {    
    struct INode* inode = &inode_table[ino_num];
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {direct block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int data_reg_idx = inode->block[blk_idx];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {first level}: data_reg_idx = %d\n", data_reg_idx);
        
        memcpy(first_level_block, buffer, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 2];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {first level}: data_reg_idx = %d\n", data_reg_idx);

        // second level block
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {second level}: data_reg_idx = %d\n", data_reg_idx);
        
        memcpy(second_level_block, buffer, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        printf("[DBUG INFO] write_block {double indirect block}: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
        // first level block
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 1];
        char* first_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {first level}: data_reg_idx = %d\n", data_reg_idx);

        // second level block
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {second level}: data_reg_idx = %d\n", data_reg_idx);

        // third level block
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        data_reg_idx = (int) second_level_block[second_level_offset * SIZE_DATA_BLK_PTR];
        char* third_level_block = data_regions[data_reg_idx].space;
        printf("[DBUG INFO] write_block {third level}: data_reg_idx = %d\n", data_reg_idx);

        memcpy(third_level_block, buffer, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }

    printf("[DBUG INFO] write_block: ino_num = %d, blk_idx = %d\n", ino_num, blk_idx);
    return -1;
}

int get_new_block() {
    for (int i = 0; i < SIZE_DBMAP; i++) {
        if (data_bitmap[i] == 0) {
            data_bitmap[i] = 1;
            memset(data_regions[i].space, 0, SIZE_PER_DATA_REGION);
            return i;
        }
    }
    
    return -1;
}

int get_new_inode() {
    for (int i = 0; i < SIZE_IBMAP; i++) {
        if (inode_bitmap[i] == 0) {
            inode_bitmap[i] = 1;
            return i;
        }
    }
    
    return -1;
}

int assign_block(int ino_num, int blk_idx) {
    struct INode* inode = &inode_table[ino_num];
    int num_blocks = inode->blocks;
    if (blk_idx != num_blocks) return -1;
    // direct
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        // first level pointer
        int data_reg_idx = get_new_block();
        if (data_reg_idx < 0) return -1;
        inode->block[blk_idx] = data_reg_idx;
        printf("[DBUG INFO] assign_block {direct data block}: %d\n", data_reg_idx);

        inode->blocks = num_blocks + 1;
        
        return 0;
    }
    // indirect
    if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        // first level pointer
        int data_reg_idx = -1;
        if (blk_idx == NUM_FIRST_LEV_PTR_PER_INODE) {
            data_reg_idx = get_new_block();
            if (data_reg_idx < 0) return -1;
            inode->block[blk_idx] = data_reg_idx;
            printf("[DBUG INFO] assign_block {direct pointer block}: %d\n", data_reg_idx);
        }
        else {
            data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 2];
        }

        // second level pointer
        char* first_level_block = data_regions[data_reg_idx].space;
        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        data_reg_idx = get_new_block();
        if (data_reg_idx < 0) return -1;
        memcpy(first_level_block + first_level_offset * SIZE_DATA_BLK_PTR, (char*) &data_reg_idx, sizeof(data_reg_idx));
        printf("[DBUG INFO] assign_block {indirect data block}: %d\n", data_reg_idx);
        inode->blocks = num_blocks + 1;
        
        return 0;
    }
    // double indirect
    if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        // first level pointer
        int data_reg_idx = -1;
        if (blk_idx == NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
            data_reg_idx = get_new_block();
            if (data_reg_idx < 0) return -1;
            inode->block[blk_idx] = data_reg_idx;
            printf("[DBUG INFO] assign_block {direct pointer block}: %d\n", data_reg_idx);
        }
        else {
            data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 1];
        }

        // second level pointer
        char* first_level_block = data_regions[data_reg_idx].space;
        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        if (second_level_offset == 0) {
            data_reg_idx = get_new_block();
            if (data_reg_idx < 0) return -1;
            memcpy(first_level_block + first_level_offset * SIZE_DATA_BLK_PTR, (char*) &data_reg_idx, sizeof(data_reg_idx));
            printf("[DBUG INFO] assign_block {indirect pointer block}: %d\n", data_reg_idx);
        }
        else {
            data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        }

        // third level pointer
        char* second_level_block = data_regions[data_reg_idx].space;
        data_reg_idx = get_new_block();
        if (data_reg_idx < 0) return -1;
        memcpy(second_level_block + second_level_offset * SIZE_DATA_BLK_PTR, (char*) &data_reg_idx, sizeof(data_reg_idx));
        printf("[DBUG INFO] assign_block {double indirect data block}: %d\n", data_reg_idx);
        
        inode->blocks = num_blocks + 1;

        return 0;
    }
    // file too large
    if (blk_idx >= NUM_ALL_LEV_PTR_PER_INODE) {
        return -EFBIG; // file too large [4]
    }

    return -1;
}

int reclaim_block(int ino_num, int blk_idx) {

    return 0;
}

int read_(int ino_num, char* buffer, size_t size, off_t offset) {
    if (offset < 0 || size < 0) return -1;
    if (offset + size == 0) return 0;
    int read_size = 0;
    int cur_offset = offset;
    int file_size = inode_table[ino_num].size;
    char blk_buff[SIZE_PER_DATA_REGION];
    while (cur_offset < file_size && read_size < size) {
        int blk_idx = cur_offset / SIZE_PER_DATA_REGION;
        int blk_offset = cur_offset % SIZE_PER_DATA_REGION;
        int read_bytes = read_block(ino_num, blk_idx, blk_buff);
        if (read_bytes != SIZE_PER_DATA_REGION) return -1;
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
    // add blocks
    int cur_block_num = inode_table[ino_num].blocks;
    if (offset < 0 || size < 0) return -1;
    if (offset + size == 0) return 0;
    int end_block_num = (offset + size - 1) / SIZE_PER_DATA_REGION + 1;
    for (int i = cur_block_num; i < end_block_num; i++) {
        int result = assign_block(ino_num, i);
        if (result < 0) return result;
    }
    
    int write_size = 0;
    int cur_offset = offset;
    char blk_buff[SIZE_PER_DATA_REGION];
    while (write_size < size) {
        int blk_idx = cur_offset / SIZE_PER_DATA_REGION;
        int blk_offset = cur_offset % SIZE_PER_DATA_REGION;
        int read_bytes = read_block(ino_num, blk_idx, blk_buff);
        if (read_bytes != SIZE_PER_DATA_REGION) return -1;
        // increment should be the minimun of (SIZE_PER_DATA_REGION - blk_offset, size - write_size)
        int increment = SIZE_PER_DATA_REGION - blk_offset < size - write_size ? SIZE_PER_DATA_REGION - blk_offset : size - write_size;
        memcpy(blk_buff + blk_offset, buffer + write_size, increment);
        int write_bytes = write_block(ino_num, blk_idx, blk_buff);
        if (write_bytes != SIZE_PER_DATA_REGION) return -1;
        cur_offset += increment;
        write_size += increment;
    }
    
    int file_size = inode_table[ino_num].size;
    inode_table[ino_num].size = (file_size > offset + size) ? file_size : offset + size;
        
    return write_size;
}

int find_dir_entry_ino(int ino_num, const char* name) {
    struct INode* inode = &inode_table[ino_num];
    if (inode->flag != 1) return -ENOTDIR; // not a directory [4]
    
    int file_size = inode->size;
    char* buffer = (char*) malloc(file_size);
    int read_bytes = read_(ino_num, buffer, file_size, 0);
    if (read_bytes != file_size) return -1;
    
    char filename[SIZE_FILENAME + 1];
    memset(filename, 0, SIZE_FILENAME + 1);
    int offset = 0;
    while (offset < file_size) {
        int sub_ino_num = (int) buffer[offset];
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
    // printf("[DBUG INFO] getattr: path = %s\n", path);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;

    struct INode* inode = &inode_table[ino_num];
    
    // st_dev is ignored [1]
    st->st_ino = ino_num; // set to inode number inside the file system [1]
    st->st_nlink = inode->links_count;
    st->st_uid = getuid(); // currently no owner user id info in inode, set to user who mounts the fs
    st->st_gid = getgid(); // currently no owner group id info in inode, set to user group who mounts the fs
    // st_rdev is ignored?
    st->st_atime = time(NULL); // currently no last access time info in inode, set to current time
    st->st_mtime = time(NULL); // currently no last modify time info in inode, set to current time
    st->st_ctime = time(NULL); // currently no last change time info in inode, set to current time
    // st_blksize is ignored
    st->st_blocks = inode->blocks; // set to number of data blocks assigned, slightly different from [2] 
    st->st_size = inode->size; // same as [2]
    
    if (inode->flag == 0) {
        st->st_mode = S_IFREG | 0644; // currently no mode info in inode, set to 644 for regular
    }
    else if (inode->flag == 1) {
        st->st_mode = S_IFDIR | 0755; // currently no mode info in inode, set to 755 for directory
    }

    return 0;
}

static int do_readdir(const char* path, void* res_buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
	// printf("[DBUG INFO] readdir: path = %s\n", path);

    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    
    struct INode* inode = &inode_table[ino_num];
    if (inode->flag != 1) return -ENOTDIR; // not a directory [4]

	filler(res_buf, ".", NULL, 0); // current Directory
	filler(res_buf, "..", NULL, 0); // parent Directory

    int file_size = inode->size;
    char* buffer = (char*) malloc(file_size);
    int read_bytes = read_(ino_num, buffer, file_size, 0);
    if (read_bytes != file_size) return -1;
    
    char filename[SIZE_FILENAME + 1];
    memset(filename, 0, SIZE_FILENAME + 1);
    int cur_offset = 0;
    while (cur_offset < file_size) {
        int sub_ino_num = (int) buffer[cur_offset];
        memcpy(filename, buffer + cur_offset + sizeof(sub_ino_num), SIZE_FILENAME);
        if (sub_ino_num >=0) filler(res_buf, filename, NULL, 0);
        cur_offset += SIZE_DIR_ITEM;
    }

    free(buffer);
    return 0;
}

static int do_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* fi) {
    printf("[DBUG INFO] read: path = %s, size = %ld, offset = %ld\n", path, size, offset);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    return read_(ino_num, buffer, size, offset);
}

static int do_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* info) {
    printf("[DBUG INFO] write: path = %s, size = %ld, offset = %ld\n", path, size, offset);
    int ino_num = get_inode_number(path);
    if (ino_num < 0) return ino_num;
    return write_(ino_num, buffer, size, offset);
}

static int do_mkdir(const char* path, mode_t mode) {
    printf("[DBUG INFO] mkidr: path = %s\n", path);

    int plen = strlen(path);

    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len < 0) return -ENOENT; // no such file or directory [4]
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
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0) return -EEXIST; // file exists [4]

    // file info
    file_ino_num = get_new_inode();
    if (file_ino_num < 0) return -1;
    struct INode* file_inode = &inode_table[file_ino_num];
    file_inode->flag = 1; // directory
    file_inode->blocks = 0;
    file_inode->links_count = 2; // direcotry has another "." file pointing to itself
    file_inode->size = 0;
    
    // parent directory info
    struct INode* parent_inode = &inode_table[parent_ino_num];
    parent_inode->links_count = parent_inode->links_count + 1; // subdir has another ".." file pointing to parent
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, (char*) &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, parent_inode->size);
    
    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static int do_mknod(const char* path, mode_t mode, dev_t rdev) {
    printf("[DBUG INFO] mknod: path = %s\n", path);

    int plen = strlen(path);

    int pos = plen - 1;
    while(path[pos] != '/') pos--;
    if (pos < 0) return -ENOENT; // no such file or directory [4]
    int file_name_len = plen - 1 - pos;
    if (file_name_len < 0) return -ENOENT; // no such file or directory [4]
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
    int file_ino_num = find_dir_entry_ino(parent_ino_num, file_name);
    if (file_ino_num >= 0) return -EEXIST; // file exists [4]

    // file info
    file_ino_num = get_new_inode();
    if (file_ino_num < 0) return -1;
    struct INode* file_inode = &inode_table[file_ino_num];
    file_inode->flag = 0; // regular
    file_inode->blocks = 0;
    file_inode->links_count = 1;
    file_inode->size = 0;
    
    // parent directory info
    struct INode* parent_inode = &inode_table[parent_ino_num];
    char new_dir_entry[SIZE_DIR_ITEM];
    memset(new_dir_entry, 0, SIZE_DIR_ITEM);
    memcpy(new_dir_entry, (char*) &file_ino_num, sizeof(file_ino_num));
    memcpy(new_dir_entry + sizeof(file_ino_num), file_name, SIZE_FILENAME);
    int write_bytes = write_(parent_ino_num, new_dir_entry, SIZE_DIR_ITEM, parent_inode->size);

    return write_bytes == SIZE_DIR_ITEM ? 0 : -1;
}

static struct fuse_operations operations = {
    .getattr	= do_getattr,
    .readdir	= do_readdir,
    .read		= do_read,
    .write		= do_write,
    .mkdir		= do_mkdir,
    .mknod		= do_mknod,
};

int main( int argc, char* argv[] ) {
    // initialize meta data
    for (int i = 0; i < SIZE_IBMAP; i++) inode_bitmap[i] = 0;
    for (int i = 0; i < SIZE_DBMAP; i++) data_bitmap[i] = 0;
    
    // initialize root inode
    inode_bitmap[ROOT_INUM] = 1;
    struct INode* root_inode = &inode_table[ROOT_INUM];
    root_inode->flag = 1; // directory
    root_inode->blocks = 0;
    root_inode->links_count = 2; // direcotry has another "." file pointing to itself
    root_inode->size = 0;

    return fuse_main(argc, argv, &operations, NULL);
}
