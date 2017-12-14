#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "directory.h"

int 
add_inode_to_directory(inode* dir_node, const char* filename, int id) {
	if (search_in_directory(dir_node, filename) >= 0) return -1;
	get_inode(id)->nlinks++;
	char* to_append = malloc(strlen(filename) + 2 + (int) (log10(id)) + 1); // filename, 2 for slashes, length of number, 1 for null terminator
	sprintf(to_append, "%s/%d/", filename, id);
	//printf("appending %s to directory file\n", to_append);
	append_to_inode(dir_node, to_append, strlen(to_append));
	free(to_append);
	return 0;
}

int
remove_inode_from_directory(inode* dir_node, int id) {
	char* data = get_inode_data(dir_node);
	dflink* head = get_directory_children(dir_node);
	if (head == NULL) return -1;
	ptrdiff_t start_offset = 0;
	ptrdiff_t offset_size = 0;
	dflink* curr = NULL;
	while (curr = (curr == NULL) ? head : curr->next) {
		if (curr->id == id) {
			offset_size = get_dflink_string_size(curr);
			break;
		}
		start_offset += get_dflink_string_size(curr);
	}
	free_dflink(head);

	if (offset_size > 0) {
		off_t new_size = dir_node->size - offset_size;
		for (off_t i = start_offset; i < new_size; i++) {
			data[i] = data[i + offset_size];
		}
		set_inode_data(dir_node, data, new_size);
	} else {
		free(data);
		return -1;
	}

	free(data);
	return 0;
}

// returns inode id of destination file
int search_in_directory(inode* dir_node, const char* filename) {
	dflink* head = get_directory_children(dir_node);
	if (head == NULL) return -1;
	dflink* curr = NULL;
	while (curr = (curr == NULL) ? head : curr->next) {
		if (strcmp(filename, curr->name) == 0) {
			int id = curr->id;
			free_dflink(head);
			return id;
		}
	}
	free_dflink(head);
	return -1;
}

dflink* 
make_dflink(const char* name, int id) {
	dflink* link = malloc(sizeof(dflink));
	link->name = malloc(strlen(name) + 1);
	memcpy(link->name, name, strlen(name) + 1);
	link->id = id;
	link->next = NULL;
	return link;
}

void 
free_dflink(dflink* link) {
	free(link->name);
	if (link->next != NULL) free_dflink(link->next);
	free(link);
}

dflink* 
get_directory_children(inode* dir) {
	char* data = get_inode_data(dir);
	char* datarest = data;
	if (strcmp(data, "") == 0) {
		free(data);
		return NULL;
	}
	char* filename = NULL;
	int id = 0;
	dflink* head = NULL;
	dflink* tail = NULL;
	while (filename = strtok_r(datarest, "/", &datarest)) {
		id = strtol(strtok_r(datarest, "/", &datarest), NULL, 10);
		dflink* link = make_dflink(filename, id);
		if (head == NULL) {
			head = link; tail = link;
		} else {
			tail->next = link;
			tail = link;
		}
	}
	free(data);
	return head;
}

int 
get_dflink_string_size(dflink* link) {
	int ret = (strlen(link->name) + (int) (ceil(log10(link->id))) + 2);
	return ret;
}
