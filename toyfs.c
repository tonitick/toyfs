#define FUSE_USE_VERSION 30

#define SIZE_IBMAP 32
#define SIZE_DBMAP 32
#define SIZE_PER_DATA_REGION 64
#define SIZE_FILENAME 12
#define ROOT_INUM 0
#define NUM_DISK_PTRS_PER_INODE 4
#define DATA_BLK_PTR_SIZE 4
#define NUM_DATA_BLK_PTR_PER_BLK (SIZE_PER_DATA_REGION / DATA_BLK_PTR_SIZE)

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
	unsigned int flag; // file type {0:REGULAR, 1:DIRECTORY, 2:SOFTLINK}
	unsigned int blocks; // number of blocks used
	unsigned int block[NUM_DISK_PTRS_PER_INODE]; // block pointers
	unsigned int links_count; // number of hard links
	unsigned int size; // file size in byte
};
struct INode inode_table[SIZE_IBMAP];

struct DataBlock {
	unsigned char space[SIZE_PER_DATA_REGION];
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
	if(blk_idx < NUM_DISK_PTRS_PER_INODE -2) {
        strncpy(buffer, data_regions[inode_table[dir_ino].block[blk_idx]], SIZE_PER_DATA_REGION);
    }
    else if (blk_idx >= NUM_DISK_PTRS_PER_INODE - 2 && blk_idx < NUM_DISK_PTRS_PER_INODE - 2 + NUM_DATA_BLK_PTR_PER_BLK) {
        char indirect_ptrs_blk[SIZE_PER_DATA_REGION];
        strncpy(buffer, data_regions[inode_table[dir_ino].block[NUM_DISK_PTRS_PER_INODE - 2]], SIZE_PER_DATA_REGION);
        int 
    }
    
}
int find_dir_entry_ino(int dir_ino, const char* name) {
    if (inode_table[dir_ino].flag != 1) return -ENOTDIR;
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
    /* Reference
       [1] fuse getattr: https://libfuse.github.io/doxygen/structfuse__operations.html#a60144dbd1a893008d9112e949300eb77
       [2] stat struct: http://man7.org/linux/man-pages/man2/stat.2.html
       [3] inode struct: http://books.gigatux.nl/mirror/kerneldevelopment/0672327201/ch12lev1sec6.html
       [4] <sys/types.h> header: http://www.doc.ic.ac.uk/~svb/oslab/Minix/usr/include/sys/types.h
    */
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

// static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
// {
// 	int file_idx = get_file_index( path );
// 	
// 	if ( file_idx == -1 )
// 		return -1;
// 	
// 	char *content = files_content[ file_idx ];
// 	
// 	memcpy( buffer, content + offset, size );
// 		
// 	return strlen( content ) - offset;
// }
// 
// static int do_mkdir( const char *path, mode_t mode )
// {
// 	path++;
// 	add_dir( path );
// 	
// 	return 0;
// }
// 
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

