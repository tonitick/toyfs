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

// void add_dir( const char *dir_name )
// {
// 	curr_dir_idx++;
// 	strcpy( dir_list[ curr_dir_idx ], dir_name );
// }
// 
// int is_dir( const char *path )
// {
// 	path++; // Eliminating "/" in the path
// 	
// 	for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
// 		if ( strcmp( path, dir_list[ curr_idx ] ) == 0 )
// 			return 1;
// 	
// 	return 0;
// }
// 
// void add_file( const char *filename )
// {
// 	curr_file_idx++;
// 	strcpy( files_list[ curr_file_idx ], filename );
// 	
// 	curr_file_content_idx++;
// 	strcpy( files_content[ curr_file_content_idx ], "" );
// }
// 
// int is_file( const char *path )
// {
// 	path++; // Eliminating "/" in the path
// 	
// 	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
// 		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
// 			return 1;
// 	
// 	return 0;
// }
// 
// int get_file_index( const char *path )
// {
// 	path++; // Eliminating "/" in the path
// 	
// 	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
// 		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
// 			return curr_idx;
// 	
// 	return -1;
// }
// 
// void write_to_file( const char *path, const char *new_content )
// {
// 	int file_idx = get_file_index( path );
// 	
// 	if ( file_idx == -1 ) // No such file
// 		return;
// 		
// 	strcpy( files_content[ file_idx ], new_content ); 
// }
// 
// // ... //
// 

int read_block(int ino_num, int blk_idx, char* buffer) {
    struct INode* inode = &inode_table[ino_num];
    // direct pointer
    if (blk_idx < NUM_FIRST_LEV_PTR_PER_INODE) {
        int data_reg_idx = inode->block[blk_idx];
        char* first_level_block = data_regions[data_reg_idx].space;
        
        strncpy(buffer, first_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // indirect pointer
    else if (blk_idx >= NUM_FIRST_LEV_PTR_PER_INODE && blk_idx < NUM_FIRST_TWO_LEV_PTR_PER_INODE) {
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 2];
        char* first_level_block = data_regions[data_reg_idx].space;

        int first_level_offset = blk_idx - NUM_FIRST_LEV_PTR_PER_INODE;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        
        strncpy(buffer, second_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    // double indirect pointer
    else if (blk_idx >= NUM_FIRST_TWO_LEV_PTR_PER_INODE && blk_idx < NUM_ALL_LEV_PTR_PER_INODE) {
        int data_reg_idx = inode->block[NUM_DISK_PTRS_PER_INODE - 1];
        char* first_level_block = data_regions[data_reg_idx].space;

        int first_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) / NUM_PTR_PER_BLK;
        data_reg_idx = (int) first_level_block[first_level_offset * SIZE_DATA_BLK_PTR];
        char* second_level_block = data_regions[data_reg_idx].space;
        
        int second_level_offset = (blk_idx - NUM_FIRST_TWO_LEV_PTR_PER_INODE) % NUM_PTR_PER_BLK;
        data_reg_idx = (int) second_level_block[second_level_offset * SIZE_DATA_BLK_PTR];
        char* third_level_block = data_regions[data_reg_idx].space;

        strncpy(buffer, third_level_block, SIZE_PER_DATA_REGION);

        return SIZE_PER_DATA_REGION;
    }
    else return -1;
}

int assign_block(int ino_num, int blk_idx) {
    // struct INode* inode = &inode_table[ino_num];
    // int 
    // if (blk_idx)
    return 0;
}

int reclaim_block(int ino_num, int blk_idx) {
    return 0;
}

int read_(int ino_num, char *buffer, size_t size, off_t offset) {
    int read_size = 0;
    int cur_offset = offset;
    int file_size = inode_table[ino_num].size;
    char blk_buff[SIZE_PER_DATA_REGION];
    while (cur_offset >= 0 && cur_offset < file_size && read_size < size) {
        int blk_idx = cur_offset / SIZE_PER_DATA_REGION;
        int blk_offset = cur_offset % SIZE_PER_DATA_REGION;
        int read_bytes = read_block(ino_num, blk_idx, blk_buff);
        if (read_bytes != SIZE_PER_DATA_REGION) return -1;
        // increment = min (read_bytes - blk_offset, file_size - cur_offset, size - read_size)
        int increment = (read_bytes - blk_offset < file_size - cur_offset) ? read_bytes - blk_offset : file_size - cur_offset;
        increment = (increment < size -  read_size) ? increment : size - read_size;
        strncpy(buffer + read_size, blk_buff + blk_offset, increment);
        cur_offset += increment;
        read_size += increment;
    }
    
    return read_size;
}

int write_(int ino_num, char *buffer, size_t size, off_t offset) {
    return 0;
}

int find_dir_entry_ino(int ino_num, const char* name) {
    if (inode_table[ino_num].flag != 1) return -ENOTDIR;
    

	return 0;
}

int get_inode_number(const char *path) {
	// get inode number from an absolute path
    int plen = strlen(path);
	int ino_num = ROOT_INUM; // root
    int lpos = 1; // bypass the preceding '/'
	while (lpos < plen) {
        int rpos = lpos;
        while (rpos < plen && path[rpos] != '/')
            rpos++;
        char name[SIZE_FILENAME + 1];
        strncpy(name, path + lpos, rpos - lpos);
        name[rpos - lpos] = 0;
		printf("[DBUG INFO] next file name: %s\n", name);
        int new_ino_num = find_dir_entry_ino(ino_num, name);
        if (new_ino_num < 0) return -ENOENT;
        ino_num = new_ino_num;
        lpos = rpos + 1;
    }
    return ino_num;
}

static int do_getattr(const char *path, struct stat *st) {
    printf("readdir input path: %s\n", path);
    int inn = get_inode_number(path);
    if (inn < 0 ) 
        return inn;
    else {
        // st_dev is ignored [1]
        // st_ino is ignored [1]
        st->st_nlink = inode_table[inn].links_count;
        // st->st_uid = getuid(); // currently no owner user id info in inode 
        // st->st_gid = getgid(); // currently no owner group id info in inode
        // st_rdev is ignored?
        st->st_atime = time(NULL); // currently no last access time info in inode, set to current time
        st->st_mtime = time(NULL); // currently no last modify time info in inode, set to current time
        st->st_ctime = time(NULL); // currently no last change time info in inode, set to current time
        // st_blksize is ignored
        st->st_blocks = inode_table[inn].blocks;
        
        if (inode_table[inn].flag == 0) {
            st->st_mode = S_IFDIR | 0755; // currently no mode info in inode, set to 755 for directory
            // no st_size definition for directory in [2]
        }
        else if (inode_table[inn].flag == 1) {
            st->st_mode = S_IFREG | 0644; // currently no mode info in inode, set to 644 for regular
            st->st_size = inode_table[inn].size;
        }
    }
    
    return 0;
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
	printf("readdir input path: %s\n", path);
	
	// filler( buffer, ".", NULL, 0 ); // Current Directory
	// filler( buffer, "..", NULL, 0 ); // Parent Directory
	// 
	// if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
	// {
	// 	for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
	// 		filler( buffer, dir_list[ curr_idx ], NULL, 0 );
	// 
	// 	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
	// 		filler( buffer, files_list[ curr_idx ], NULL, 0 );
	// }
	// 
    return 0;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi ) {
    return 0;
}

static int do_mkdir( const char *path, mode_t mode )
{
	
	return 0;
}

// static int do_mknod( const char *path, mode_t mode, dev_t rdev )
// {
// 	path++;
// 	add_file( path );
// 	
// 	return 0;
// }
// 
// static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info )
// {
// 	write_to_file( path, buffer );
// 	
// 	return size;
// }
// 
static struct fuse_operations operations = {
    .getattr	= do_getattr,
    // .readdir	= do_readdir,
    // .read		= do_read,
    // .mkdir		= do_mkdir,
    // .mknod		= do_mknod,
    // .write		= do_write,
};

int main( int argc, char *argv[] )
{
	return fuse_main( argc, argv, &operations, NULL );
	// return 0;
}

