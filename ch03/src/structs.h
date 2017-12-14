#include <stddef.h>

#ifndef STRUCTS_H
#define STRUCTS_H

#define BLOCK_SIZE 4096
//#define MAX_FILE_SIZE (BLOCK_SIZE * (10 + BLOCK_SIZE/sizeof(unsigned int)))
#define NUM_DIR_BLOCKS 10

#define DEFAULT_FILE_MODE 0100644
#define DEFAULT_DIR_MODE 040755

typedef struct data_block {
    char data[BLOCK_SIZE];
} data_block;

typedef struct file_block {
    char data[BLOCK_SIZE];
} file_block;

typedef struct extended_block {
	unsigned int data[BLOCK_SIZE/sizeof(unsigned int)];
} extended_block;

typedef struct inode {
    mode_t mode;        // read/write/execute
    uid_t uid;			// user id
    gid_t gid;			// group id
    off_t size;         // in bytes
    time_t ctime;       // created time
    time_t mtime;       // modified time
    time_t atime;       // accessed time
    nlink_t nlinks;    	// number of hard links
    unsigned int nblocks;                   // number of blocks
    unsigned int blocks[NUM_DIR_BLOCKS];    // block numbers
	unsigned int indirect_block;            // extended block number (for files larger than 10 blocks)
} inode;

typedef struct super_block {
	ptrdiff_t inode_bitmap;		// pointer to inodes bitmap
	ptrdiff_t block_bitmap;		// pointer to blocks bitmap
	ptrdiff_t inodes;			// pointer to inodes segment
	ptrdiff_t blocks;			// pointer to blocks segment
	unsigned int ninodes;		// number of inodes
	unsigned int nblocks;		// number of blocks
	unsigned int root_inode;	// id of the root inode directory
} super_block;

// gets the number of nodes and blocks that will fit in a given sized file system
unsigned int 
get_num_nodes(size_t size);

#endif
