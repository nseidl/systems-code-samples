#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "structs.h"

void
storage_init(const char* path);
void
set_inode_size(inode* node, off_t size);
void
append_to_inode(inode* node, const char* data, off_t size);
char
get_inode_byte(inode* node, off_t n);
void
set_inode_byte(inode* node, off_t n, char byte);
char*
get_inode_data(inode* node);
void
set_inode_data(inode* node, const char* data, off_t size);
int
get_inode_from_path(const char* path);
int
make_file(inode* dir, const char* filename, mode_t mode);
int
get_stat(const char* path, struct stat* st);
const char*
get_data(const char* path);
inode*
get_inode(int id);
void
clear_inode(int id);

#endif
