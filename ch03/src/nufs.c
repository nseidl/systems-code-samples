// Skeleton design and implementation by Nat Tuck.
// Complete design and implementation by Nick Patella and Nick Seidl
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <bsd/string.h>
#include <assert.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "directory.h"
#include "bitmap.h"
#include "structs.h"

// 1 if inode exists; -1 if inode doesn't exist
int 
exists(const char *path) {
    int id = get_inode_from_path(path);
    if (id < 0) {
        printf("file <%s> not found\n", path);
        return -1;
    } else {
        return 1;
    }
}

int
min(int a, int b) {
	return (a < b) ? a : b;
}

// produes path to the parent inode directory, given a full path
char* get_parent_path(const char* path) {
	char* last_slash = strrchr(path, '/');
    int dir_length = last_slash - path + 1;
    char* dir = malloc(dir_length + 1);
	dir[dir_length] = '\0';
    strncpy(dir, path, dir_length);
	return dir;
}


// checks if file exists
int
nufs_access(const char *path, int mask)
{
    printf("access(%s, %04o)\n", path, mask);
    int rv = exists(path);

    if (rv == 1) {
        get_inode(get_inode_from_path(path))->atime = time(NULL);
        rv = 0;
    } 

    return rv;
}

// populats the given stat struct with information from the inode
// OR, if inode doesn't exist, produces error
int
nufs_getattr(const char *path, struct stat *st)
{
    printf("getattr(%s)\n", path);

    int rv = get_stat(path, st);

    if (rv < 0) {
        return -ENOENT; 
    }

    get_inode(get_inode_from_path(path))->atime = time(NULL);

    return 0;
}

// populates filler repeatedly with directory entries
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    // buffer of directory information
    struct stat st;
    struct stat parent_st;

    printf("readdir(%s)\n", path);

    if (exists(path) != 1) {
        return -ENOENT;
    }

    inode* dir_node = get_inode(get_inode_from_path(path));
    dflink* head = get_directory_children(dir_node);
    dflink* curr = head;

    get_stat(path, &st);
    filler(buf, ".", &st, 0);

    char* parent_path = get_parent_path(path);
    if (strcmp(path, "/") != 0) {
        get_stat(parent_path, &parent_st);
        filler(buf, "..", &parent_st, 0);
    }
    free(parent_path);

    // iterate through all immediate children in this directory
    if (head != NULL) {
        while (curr != NULL) {
            get_stat(curr->name, &st);
            filler(buf, curr->name, &st, 0);
            curr = curr->next;
        }
    } 

    return 0;
}


// makes a file
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("mknod(%s, %04o)\n", path, mode);

    // empty path
    if (strlen(path) < 1) {
        return -1;
    }

	// extract the directory from path
    char* dir = get_parent_path(path);

	// the folder does not exist
	if (exists(dir) == -1) {
		return -EEXIST;
	}
    // this file already exists
    if (exists(path) == 1) {
        return -EEXIST;
    }

    // extract the filename from path
    char* filename = strrchr(path, '/') + 1;

    printf("filename=%s\n", filename);
    printf("dir_path=%s\n", dir);

    inode* dir_inode = get_inode(get_inode_from_path(dir));

	free(dir);

    if (make_file(dir_inode, filename, mode) > -1) {
        return 0;
    } else {
        return -EIO;
    }
}

// makes a directory
int
nufs_mkdir(const char *path, mode_t mode)
{
    printf("mkdir(%s, %04o)\n", path, mode);

    // empty path
    if (strlen(path) < 1) {
        return -1;
    }

    // extract the directory from path
    char* dir = get_parent_path(path);

    // the folder does not exist
    if (exists(dir) == -1) {
        return -EEXIST;
    }
    // this file already exists
    if (exists(path) == 1) {
        return -EEXIST;
    }

    // extract the filename from path
    char* filename = strrchr(path, '/') + 1;

    printf("filename=%s\n", filename);
    printf("dir_path=%s\n", dir);

    inode* dir_inode = get_inode(get_inode_from_path(dir));

    printf("size=%ld\n", dir_inode->size);

    free(dir);
    if (make_file(dir_inode, filename, mode) > -1) {
        return 0;
    } else {
        return -EIO;
    }
}

int nufs_link(const char* from, const char* to) {
	printf("link(%s => %s)\n", from, to);
	
	char* to_dir = get_parent_path(to);

	int to_dir_id = get_inode_from_path(to_dir);
	// hardlink dir doesn't exist
	if (to_dir_id < 0) {
		return -EEXIST;
	}

	int from_file_id = get_inode_from_path(from);
	// hardlink target doesn't exist
	if (from_file_id < 0) {
		return -EEXIST;
	}
	inode* from_node = get_inode(from_file_id);

	char* to_filename = strrchr(to, '/') + 1;
	
	add_inode_to_directory(get_inode(to_dir_id), to_filename, from_file_id);
	
    return 0;
}

// removes a file
int 
nufs_unlink(const char *path)
{
    printf("unlink(%s)\n", path);

    if (exists(path) != 1) {
        return -ENOENT;
    }

    int id = get_inode_from_path(path);
    inode* node = get_inode(id);

    int dir_id = get_inode_from_path(get_parent_path(path));
    inode* dir_node = get_inode(dir_id);

    remove_inode_from_directory(dir_node, id);

    // if nobody else points to this node, we are fine purging it
	node->nlinks--;
    if (node->nlinks == 0) {
        clear_inode(id);
    }

    return 0;
}

// removes a directory
int
nufs_rmdir(const char *path)
{
    printf("rmdir(%s)\n", path);

    if (exists(path) != 1) {
        return -ENOENT;
    }

    int id = get_inode_from_path(path);
    inode* node = get_inode(id);

    int dir_id = get_inode_from_path(get_parent_path(path));
    inode* dir_node = get_inode(dir_id);

    printf("size=%ld\n", dir_node->size);

    if (node->size == 0) {
        remove_inode_from_directory(dir_node, id);

        // if nobody else points to this node, we are fine purging it
        if (node->nlinks > 0) {
            node->nlinks--;
        } else {
            clear_inode(id);
        }

        return 0;
    }

    return -ENOTEMPTY;
}

// renames a file
int
nufs_rename(const char *from, const char *to)
{
    printf("rename(%s => %s)\n", from, to);
	
	char* from_dir = get_parent_path(from);
	char* to_dir = get_parent_path(to);

	int from_dir_id = get_inode_from_path(from_dir);
	int to_dir_id = get_inode_from_path(to_dir);

	if (from_dir_id < 0 || to_dir_id < 0) {
		return -EEXIST;
	}

	int from_file_id = get_inode_from_path(from);
	if (from_file_id < 0) {
		return -EEXIST;
	}

	char* from_filename = strrchr(from, '/') + 1;
	char* to_filename = strrchr(to, '/') + 1;

	remove_inode_from_directory(get_inode(from_dir_id), from_file_id);
	add_inode_to_directory(get_inode(to_dir_id), to_filename, from_file_id);
	
    return 0;
}

// changs mode of path
int
nufs_chmod(const char *path, mode_t mode)
{
    printf("chmod(%s, %04o)\n", path, mode);

    int rv = exists(path);

    if (rv == 1) {
        get_inode(get_inode_from_path(path))->mode = mode;
        rv = 0;
    }

    return rv;
}

// sets a file's size
int
nufs_truncate(const char *path, off_t size)
{
    printf("truncate(%s, %ld bytes)\n", path, size);

    int rv = exists(path);

    if (rv == 1) {
        set_inode_size(get_inode(get_inode_from_path(path)), (size_t) size);
        return 0;
    } else {
        return -ENOENT;
    }
}


// checks to see if file exists; if it does, update access time
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    printf("open(%s)\n", path);

    int rv = exists(path);

    if (rv == 1) {
        get_inode(get_inode_from_path(path))->atime = time(NULL);
        return 0;
    } else {
        return -ENOENT;
    }
}

// fills buf with data from path starting at offset
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read(%s, %ld bytes, %ld)\n", path, size, offset);
	int rv = exists(path);

    if (rv == -1) {
        return -ENOENT;
    }
	
	inode* node = get_inode(get_inode_from_path(path));

    char* data = get_inode_data(node);
	char* read_from = data + min(offset, node->size);

    off_t read_until = min(node->size, offset + size);
	memcpy(buf, read_from, read_until - offset);

	free(data);
    return (read_until - offset);
}

// writes data from buf to path starting at offset
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read(%s, %ld bytes, %ld)\n", path, size, offset);
	int rv = exists(path);

    if (rv == -1) {
        return -ENOENT;
    }
	
	inode* node = get_inode(get_inode_from_path(path));
    char* data = get_inode_data(node);

    off_t new_size = offset + size;
	char* new_data = malloc(new_size); 
	
	memcpy(new_data, data, node->size);
	memcpy(new_data + offset, buf, size);

	set_inode_data(node, new_data, new_size);

	free(data);

    return size;
}

// updates access and modified times on file
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    printf("utimens(%s, [%lds, %lds])\n",
           path, ts[0].tv_sec, ts[1].tv_sec);
    int rv = exists(path);

    if (rv == 1) {
        int id = get_inode_from_path(path);
        get_inode(id)->atime = ts[0].tv_sec;
        get_inode(id)->mtime = ts[1].tv_sec; 
        return 0;
    } else {
        return -ENOENT;
    }
}

// lots of function pointers
void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
	ops->link	  = nufs_link;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open     = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    storage_init(argv[--argc]); // pass through the "data.nufs" disk image name
    nufs_init_ops(&nufs_ops); // fill in nufs_ops global variable
    return fuse_main(argc, argv, &nufs_ops, NULL);

    // when a fuse event happens, it's going to call events in our struct
}
