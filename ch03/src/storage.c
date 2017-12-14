// Skeleton design and implementation by Nat Tuck.
// Complete design and implementation by Nick Patella and Nick Seidl
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <sys/param.h>
#include <assert.h>
#include <time.h>

#include "storage.h"
#include "structs.h"
#include "bitmap.h"
#include "directory.h"

static super_block* super;

static void* inode_bitmap;
static void* block_bitmap;
static void* inodes;
static void* blocks;

static void* disk;
static size_t disk_size;
static int disk_fd;

void
set_block_bit(unsigned int i, int val) {
	set_bit(block_bitmap, i, val);
}

int
get_block_bit(unsigned int i, int val) {
	return get_bit(block_bitmap, i);
}

void
set_inode_bit(unsigned int i, int val) {
	set_bit(inode_bitmap, i, val);
}

int
get_inode_bit(unsigned int i, int val) {
	return get_bit(inode_bitmap, i);
}

int
get_new_inode() {
	for (int i = 0; i < super->ninodes; i++) {
		if (get_bit(inode_bitmap, i) == 0) {
			set_bit(inode_bitmap, i, 1);
			return i;
		}
	}
	return -1; // not found
}

void 
clear_inode(int id) {
	set_inode_bit(id, 0);
}

int
get_new_block() {
	for (int i = 0; i < super->nblocks; i++) {
		if (get_bit(block_bitmap, i) == 0) {
			set_block_bit(i, 1);
			return i;
		}
	}
	return -1; // not found
}

void
check_rv(int rv) {
  if (rv < 0) {
    perror("error");
    abort();
  }
}

int get_inode_block(inode* node, int n) {
    if (n < 0 || n >= node->nblocks) {
        perror("Tried to index a nonexistent block.");
        abort();
    }
    if (n < NUM_DIR_BLOCKS) {
        return node->blocks[n];
    } else {
        return ((data_block*) blocks)[node->indirect_block].data[n - NUM_DIR_BLOCKS];
    }
}

inode*
get_inode(int id) {
	return ((inode*)inodes + id);
}

// returns inode id of new inode
int
make_file(inode* dir, const char* filename, mode_t mode) {
    int id = get_new_inode();
	inode* node = get_inode(id);
	
	if (mode == 33204) {
		node->mode = DEFAULT_FILE_MODE;
	} else if (mode == 509) {
		node->mode = DEFAULT_DIR_MODE;
	} else {
		node->mode = mode;
	}
	//node->mode = mode;

	node->uid = getuid();
	node->gid = getgid();
	time_t t = time(NULL);
	node->atime = t;
	node->ctime = t;
	node->mtime = t;
	node->size = 0;
    add_inode_to_directory(dir, filename, id);
    return id;
}

// path: name of 1 megabyte disk image
void
storage_init(const char* path)
{
	int rv;
	size_t fsize;
	int flags = O_RDWR;
	int is_new_file = 0;
	rv = access(path, F_OK);
	if (rv == 0) { // file exists
		// get size of file	
		struct stat st;
		rv = stat(path, &st);
		check_rv(rv);
		fsize = st.st_size;
	} else { // file does not exist
		fsize = (size_t) pow(2, 20); // one megabyte
		flags = flags | O_CREAT; // create file
		is_new_file = 1;
	}

	// LIMIT TO ONE MEGABYTE 
	if (fsize != (size_t) pow(2, 20)) {
		perror("File must be one megabyte.");
		abort();
	}

	int fd = open(path, flags, 0644);
    check_rv(fd);

    // cap file size to 1,048,576
    rv = ftruncate(fd, fsize);
    check_rv(rv);

	void* file = mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // load the file
	super = file;
	
	inode* root_inode;
	if (is_new_file) {
		super->ninodes = get_num_nodes(fsize);
        super->nblocks = super->ninodes;
        super->root_inode = 0;
        // set offsets to mirror ext2 filesystem
        // | super_block | inode bitmap | block bitmap | inodes | blocks |
        inode_bitmap = file + sizeof(super_block);
        block_bitmap = inode_bitmap + super->ninodes / 4  + !!(super->ninodes % 4); 
        inodes = block_bitmap + super->nblocks / 4  + !!(super->nblocks % 4); 
        blocks = inodes + super->ninodes * sizeof(inode);

		super->inode_bitmap = inode_bitmap - (void*) super;
		super->block_bitmap = block_bitmap - (void*) super;
		super->inodes = inodes - (void*) super;
		super->blocks = blocks - (void*) super;

		// set up root directory
		super->root_inode = get_new_inode();
		root_inode = ((inode*) inodes + super->root_inode);
		root_inode->mode = DEFAULT_DIR_MODE;	
		root_inode->uid = getuid();
		root_inode->gid = getgid();
		time_t t = time(NULL);
		root_inode->ctime = t;
		root_inode->atime = t;
		root_inode->mtime = t;
	} else {

		inode_bitmap = (void*) super + super->inode_bitmap;
		block_bitmap = (void*) super + super->block_bitmap;
		inodes = (void*) super + super->inodes;
		blocks = (void*) super + super->blocks;

		root_inode = ((inode*) inodes) + super->root_inode;

		root_inode->atime = time(NULL);
	}

	disk = file;
	disk_size = fsize;
	disk_fd = fd;

	msync(disk, disk_size, MS_SYNC);
}

char 
get_inode_byte(inode* node, off_t n) {
	int block = get_inode_block(node, n/BLOCK_SIZE);
	return ((file_block*)blocks)[block].data[n%BLOCK_SIZE];
}

void 
set_inode_byte(inode* node, off_t n, char byte) {
	int block = get_inode_block(node, n/BLOCK_SIZE);
	((file_block*)blocks)[block].data[n%BLOCK_SIZE] = byte;
}

char* 
get_inode_data(inode* node) {
	char* data = malloc(node->size + 1);
	data[node->size] = '\0'; // escape, just in case
	for (off_t i = 0; i < node->size; i++) {
		data[i] = get_inode_byte(node, i);
	}
	return data;
}

void 
set_inode_data(inode* node, const char* data, off_t size) {
	set_inode_size(node, size);
	for (off_t i = 0; i < node->size; i++) {
		set_inode_byte(node, i, data[i]);
	}
}

void
set_inode_size(inode* node, off_t size) {
	int new_nblocks = size/BLOCK_SIZE + !!(size % BLOCK_SIZE);
	if (node->nblocks == new_nblocks) {
		// do nothing?
	} else if (node->nblocks < new_nblocks) {
		// increase number of blocks
		if (node->nblocks < NUM_DIR_BLOCKS && new_nblocks <= NUM_DIR_BLOCKS) { // nblocks is under threshold and new nblocks is at or under threshold
			for (int i = node->nblocks; i < new_nblocks; i++) {
				node->blocks[i] = get_new_block();
			}
		} else if (node->nblocks < NUM_DIR_BLOCKS && new_nblocks > NUM_DIR_BLOCKS) { // nblocks has not reached threshold, new nblocks is over threshold
			node->indirect_block = get_new_block(); // create indirect block
			for (int i = node->nblocks; i < NUM_DIR_BLOCKS; i++) {
				node->blocks[i] = get_new_block();
			}
			for (int i = 0; i < new_nblocks - NUM_DIR_BLOCKS; i++) {
				((data_block*) blocks)[node->indirect_block].data[i] = get_new_block();
			}
		} else if (node->nblocks >= NUM_DIR_BLOCKS && new_nblocks > NUM_DIR_BLOCKS) { // nblocks is at or over threshold, new blocks is also over threshold
			for (int i = node->nblocks - NUM_DIR_BLOCKS; i < new_nblocks - NUM_DIR_BLOCKS; i++) {
				((data_block*) blocks)[node->indirect_block].data[i] = get_new_block();
			}
		}
	} else if (node->nblocks > new_nblocks) {
		// decrease number of blocks
		// dealcrease from node->nblocks to new_nblocks
		if (node->nblocks <= NUM_DIR_BLOCKS && new_nblocks < NUM_DIR_BLOCKS) {
			for (int i = node->nblocks; i < new_nblocks; i++) {
				set_block_bit(node->blocks[i], 0); // mark block as free
			}
		} else if (node->nblocks > NUM_DIR_BLOCKS && new_nblocks < NUM_DIR_BLOCKS) {
			for (int i = new_nblocks; i < NUM_DIR_BLOCKS; i++) {
				set_block_bit(node->blocks[i], 0); // mark block as free
			}
			for (int i = 0; i < node->nblocks - NUM_DIR_BLOCKS; i++) {
				set_block_bit(((data_block*) blocks)[node->indirect_block].data[i], 0);
			}
			set_block_bit(node->indirect_block, 0); // mark indirect block as free
		} else if (node->nblocks > NUM_DIR_BLOCKS && new_nblocks >= NUM_DIR_BLOCKS) {
			for (int i = node->nblocks - NUM_DIR_BLOCKS; i < new_nblocks - NUM_DIR_BLOCKS; i++) {
				set_block_bit(((data_block*) blocks)[node->indirect_block].data[i], 0);
			}
		}	
	}

	node->nblocks = new_nblocks;
	node->size = size;
}

void
append_to_inode(inode* node, const char* data, off_t size) {
	size_t start_pos = node->size;
	set_inode_size(node, start_pos + size);
	data_block* current_block = ((data_block*) blocks) + get_inode_block(node, start_pos/BLOCK_SIZE);
	
    for (off_t i = start_pos; i < start_pos + size; i++) {
		if (i%4096 == 0) current_block = ((data_block*) blocks) + get_inode_block(node, i/BLOCK_SIZE);
		current_block->data[i%4096] = data[i - start_pos]; 
	}
}

int
get_inode_from_path(const char* path) {
	if (strcmp(path, "/") == 0) {
		return super->root_inode;
	}

	char* pathcpy = malloc(strlen(path) + 1);
	char* pathrest = pathcpy;
	memcpy(pathcpy, path, strlen(path) + 1);
	char* token = NULL;
	int id = super->root_inode;
	inode* cur_node = get_inode(super->root_inode);
	while (token = strtok_r(pathrest, "/", &pathrest)) {
		if (S_ISDIR(cur_node->mode)) {
			id = search_in_directory(cur_node, token);
			if (id >= 0) {
				cur_node = ((inode*)inodes) + id;
			} else {
				// ERROR FILE NOT FOUND
				return -1;
			}
		} else {
			if (strtok(NULL, "/") == NULL) {
				return id;
			} else {
				return -2;
			}
		}
	}
	free(pathcpy);
    return id;
}

int
get_stat(const char* path, struct stat* st)
{
	int id = get_inode_from_path(path);

	if (id < 0) {
		return -1;
	} 

	inode* node = get_inode(id);
	st->st_mode = node->mode;
	st->st_ino = id;
	st->st_nlink = node->nlinks;
	st->st_uid = node->uid;
	st->st_gid = node->gid;
	st->st_size = node->size;
	st->st_blksize = BLOCK_SIZE;
	st->st_blocks = node->nblocks * 8;
	st->st_rdev = 0;

	struct timespec atime;
	atime.tv_sec = node->atime;
	atime.tv_nsec = node->atime * pow(10, 9); // seconds * 10^9 = milliseconds

	struct timespec mtime;
	mtime.tv_sec = node->mtime;
	mtime.tv_nsec = node->mtime * pow(10, 9);

	struct timespec ctime;
	ctime.tv_sec = node->ctime;
	ctime.tv_nsec = node->ctime * pow(10, 9);

	st->st_atim = atime;
	st->st_ctim = ctime;
	st->st_mtim = mtime;

	printf("~~~<%s>~~~\n", path);
	printf("atime = <%ld>\n", node->atime);
	printf("mode = <%04o>\n", node->mode);
	printf("data = <%s>\n", get_data(path));
	printf("~~~~~~~~~~\n");

    return 0;
}

const char*
get_data(const char* path)
{
    return get_inode_data(get_inode(get_inode_from_path(path)));
}
