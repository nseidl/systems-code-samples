#include "storage.h"
#include "structs.h"

#ifndef DIRECTORY_H
#define DIRECTORY_H

typedef struct dflink { // dir file link
	char* name;
	int id;
	struct dflink* next;
} dflink;

dflink* make_dflink(const char* name, int id);

void free_dflink(dflink* link);

dflink* get_directory_children(inode* dir);

int add_inode_to_directory(inode* dir_node, const char* filename, int id);

int
remove_inode_from_directory(inode* dir_node, int id);

// returns inode id of destination file
int search_in_directory(inode* dir_node, const char* filename);

int get_dflink_string_size(dflink* link);

#endif
