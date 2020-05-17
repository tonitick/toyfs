#define FUSE_USE_VERSION 29


#include "util.h"
#include <signal.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#define SIZE_DIR_ITEM (SIZE_FILENAME + 4) // size of directory item, 4 bytes for inode number
#define SIZE_DATA_BLK_PTR 4 // size of data block pointers

#define NUM_PTR_PER_BLK (SIZE_BLOCK / SIZE_DATA_BLK_PTR)
#define NUM_FIRST_LEV_PTR_PER_INODE (NUM_DISK_PTRS_PER_INODE - 2)
#define NUM_SECOND_LEV_PTR_PER_INODE NUM_PTR_PER_BLK
#define NUM_THIRD_LEV_PTR_PER_INODE NUM_PTR_PER_BLK * NUM_PTR_PER_BLK
#define NUM_FIRST_TWO_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE)
#define NUM_ALL_LEV_PTR_PER_INODE (NUM_FIRST_LEV_PTR_PER_INODE + NUM_SECOND_LEV_PTR_PER_INODE + NUM_THIRD_LEV_PTR_PER_INODE)

void sigint_handler(int sig) {
    printf("Caught signal %d\n", sig);
    printf("SIGINT caught, writing dirty cache back to device ...\n");
    
    printf("done\n");
    
    // exit(0);
}

char dir_list[ 256 ][ 256 ];
int curr_dir_idx = -1;

char files_list[ 256 ][ 256 ];
int curr_file_idx = -1;

char files_content[ 256 ][ 256 ];
int curr_file_content_idx = -1;

void add_dir( const char *dir_name )
{
	curr_dir_idx++;
	strcpy( dir_list[ curr_dir_idx ], dir_name );
}

int is_dir( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
		if ( strcmp( path, dir_list[ curr_idx ] ) == 0 )
			return 1;
	
	return 0;
}

void add_file( const char *filename )
{
	curr_file_idx++;
	strcpy( files_list[ curr_file_idx ], filename );
	
	curr_file_content_idx++;
	strcpy( files_content[ curr_file_content_idx ], "" );
}

int is_file( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
			return 1;
	
	return 0;
}

int get_file_index( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
			return curr_idx;
	
	return -1;
}

void write_to_file( const char *path, const char *new_content )
{
	int file_idx = get_file_index( path );
	
	if ( file_idx == -1 ) // No such file
		return;
		
	strcpy( files_content[ file_idx ], new_content ); 
}

// ... //

static int do_getattr( const char *path, struct stat *st )
{
	st->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
	st->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
	st->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	st->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
	
	if ( strcmp( path, "/" ) == 0 || is_dir( path ) == 1 )
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
	}
	else if ( is_file( path ) == 1 )
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
	}
	else
	{
		return -ENOENT;
	}
	
	return 0;
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory
	
	if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
	{
		for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
			filler( buffer, dir_list[ curr_idx ], NULL, 0 );
	
		for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
			filler( buffer, files_list[ curr_idx ], NULL, 0 );
	}
	
	return 0;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
	int file_idx = get_file_index( path );
	
	if ( file_idx == -1 )
		return -1;
	
	char *content = files_content[ file_idx ];
	
	memcpy( buffer, content + offset, size );
		
	return strlen( content ) - offset;
}

static int do_mkdir( const char *path, mode_t mode )
{
	path++;
	add_dir( path );
	
	return 0;
}

static int do_mknod( const char *path, mode_t mode, dev_t rdev )
{
	path++;
	add_file( path );
	
	return 0;
}

static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info )
{
	write_to_file( path, buffer );
	
	return size;
}

static struct fuse_operations operations = {
    .getattr	= do_getattr,
    .readdir	= do_readdir,
    .read		= do_read,
    .mkdir		= do_mkdir,
    .mknod		= do_mknod,
    .write		= do_write,
};

int main(int argc, char *argv[])
{
    queue = create_cache_queue(1000);
    hash = create_hash_table(100);
    

    int result = get_superblock();
    if (result < 0) return -1;

    // checking macros
	printf("SIZE_IBMAP = %d\n", SIZE_IBMAP);
	printf("SIZE_DBMAP = %d\n", SIZE_DBMAP);
	printf("SIZE_INODE = %d\n", SIZE_INODE);
	printf("SIZE_FILENAME = %d\n", SIZE_FILENAME);
	printf("ROOT_INUM = %d\n", ROOT_INUM);
	printf("NUM_DISK_PTRS_PER_INODE = %d\n\n", NUM_DISK_PTRS_PER_INODE);
	
	printf("NUM_INODE = %d\n", NUM_INODE);
	printf("NUM_DATA_BLKS = %d\n\n", NUM_DATA_BLKS);
	
	printf("NUM_BLKS_SUPERBLOCK = %d\n", NUM_BLKS_SUPERBLOCK);
	printf("NUM_BLKS_IMAP = %d\n", NUM_BLKS_IMAP);
	printf("NUM_BLKS_DMAP = %d\n", NUM_BLKS_DMAP);
	printf("NUM_BLKS_INODE_TABLE = %d\n\n", NUM_BLKS_INODE_TABLE);

	printf("SUPERBLOCK_START_BLK = %d\n", SUPERBLOCK_START_BLK);
	printf("IMAP_START_BLK = %d\n", IMAP_START_BLK);
	printf("DMAP_START_BLK = %d\n", DMAP_START_BLK);
	printf("INODE_TABLE_START_BLK = %d\n", INODE_TABLE_START_BLK);
	printf("DATA_REG_START_BLK = %d\n\n", DATA_REG_START_BLK);

	printf("SIZE_DIR_ITEM = %d\n", SIZE_DIR_ITEM);
	printf("SIZE_DATA_BLK_PTR = %d\n\n", SIZE_DATA_BLK_PTR);
	
	printf("NUM_PTR_PER_BLK = %d\n", NUM_PTR_PER_BLK);
	printf("NUM_FIRST_LEV_PTR_PER_INODE = %d\n", NUM_FIRST_LEV_PTR_PER_INODE);
	printf("NUM_SECOND_LEV_PTR_PER_INODE = %d\n", NUM_SECOND_LEV_PTR_PER_INODE);
	printf("NUM_THIRD_LEV_PTR_PER_INODE = %d\n", NUM_THIRD_LEV_PTR_PER_INODE);
	printf("NUM_FIRST_TWO_LEV_PTR_PER_INODE = %d\n", NUM_FIRST_TWO_LEV_PTR_PER_INODE);
	printf("NUM_ALL_LEV_PTR_PER_INODE = %d\n\n", NUM_ALL_LEV_PTR_PER_INODE);

	printf("INODE_FLAG_OFF = %d\n", INODE_FLAG_OFF);
	printf("INODE_NUM_BLKS_OFF = %d\n", INODE_NUM_BLKS_OFF);
	printf("INODE_USED_SIZE_OFF = %d\n", INODE_USED_SIZE_OFF);
	printf("INODE_LINKS_COUNT_OFF = %d\n", INODE_LINKS_COUNT_OFF);
	printf("INODE_BLK_PTR_OFF = %d\n\n", INODE_BLK_PTR_OFF);


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

    return 0;
}

