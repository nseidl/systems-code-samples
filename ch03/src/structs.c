#include <sys/types.h>
#include "structs.h"

#define BLOCK_SIZE 4096
#define MAX_FILE_SIZE (BLOCK_SIZE * (10 + BLOCK_SIZE/sizeof(unsigned int)))
#define NUM_DIR_BLOCKS 10

// gets the number of nodes and blocks that will fit in a given sized file system
unsigned int 
get_num_nodes(size_t size) {
	return (size*4 - sizeof(super_block)*4)/(sizeof(inode)*4 + sizeof(file_block)*4 + 2);
}
