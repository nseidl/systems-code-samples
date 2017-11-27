// cs3650 starter code
// Modified by Nicholas Patella and Nick Seidl and Branden Lee
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "hpmalloc.h"

typedef struct free_node {
    size_t size;
    struct free_node* prev;
    struct free_node* next;
    pthread_t id;
} free_node;

typedef struct alloc_header {
    size_t size;
    pthread_t id;
} alloc_header;

static const size_t NODE_SIZE = sizeof(free_node);
static const size_t HEADER_SIZE = sizeof(alloc_header);

const size_t PAGE_SIZE = 16384;
const size_t MMAP_SIZE = 16777216; // 16 megabytes

static const int NUM_BINS = 8;
static size_t bin_sizes[8] = {64, 128, 256, 512, 1024, 2048, 4096, 8192};
__thread free_node* local_bins[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static int get_bin(size_t size) {
    for (int i = 0; i < NUM_BINS; i++) {
        if (size  + HEADER_SIZE < bin_sizes[i]) {
            // fit in a bin -> local
            return i;
        }
    }
    // did not fit in a bin -> global
    return NUM_BINS;
}

free_node* global_free_list = NULL;
static pthread_mutex_t global_free_list_mutex = PTHREAD_MUTEX_INITIALIZER;

free_node* global_mailbox = NULL;
static pthread_mutex_t global_mailbox_mutex = PTHREAD_MUTEX_INITIALIZER;

void free_node_link(free_node* a, free_node* b) {
    if (a != NULL) a->next = b;
    if (b != NULL) b->prev = a;
}

void
free_list_insert(free_node** free_head_ptr, free_node* node) {
    free_node_link(node, *free_head_ptr);
    *free_head_ptr = node;
}

void free_list_remove(free_node** free_head_ptr, free_node* this) {
    if (this == *free_head_ptr) {
        *free_head_ptr = this->next;
    } 

    free_node_link(this->prev, this->next);
}

void free_node_combine(free_node* a, free_node* b) {
    if ((void*) a + a->size + (sizeof(free_node) - sizeof(size_t)) == (void*) b) { // CAST TO VOID POINTER SO IT COUNTS IN BYTES
        // coalesce
        a->size += b->size + sizeof(free_node) - sizeof(size_t);
        // continue, don't update previous
        free_node_link(a, b->next);
    }
}

void
free_list_coalesce(free_node* node) {
    if (node->next != NULL) {
        free_node_combine(node, node->next);
    }

    if (node->prev != NULL) {
        free_node_combine(node->prev, node);
    }
}

static
size_t
div_up(size_t xx, size_t yy)
{
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void
create_free_node(void* node_ptr, size_t size, pthread_t id) {
    *(free_node *) node_ptr = (free_node) {.size = size, .next = NULL, .prev = NULL, .id = id};
}

static void* mmap_ptr = NULL;
static size_t mmap_left = 0;
static pthread_mutex_t mmap_wrapper_mutex = PTHREAD_MUTEX_INITIALIZER;

// massive malloc size to save syscalls
void*
mmap_wrapper(size_t alloc_size) {    
    pthread_mutex_lock(&mmap_wrapper_mutex);

    if (mmap_ptr == NULL || mmap_left < alloc_size) {
        mmap_ptr = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); 
        mmap_left = MMAP_SIZE;
    } 

    void* tmp = mmap_ptr;

    mmap_ptr += alloc_size;
    mmap_left -= alloc_size;

    pthread_mutex_unlock(&mmap_wrapper_mutex);

    return tmp;
}

// takes a free_list, an available free node, and the size of the data to be allocated
// if the node is NULL then will mmap new memory, otherwise it will fill in the node's space as best as possible and add a new free node to the free list if possible
void* allocate(free_node** free_head_ptr, free_node* node, size_t size) {
    size_t avail_space;
    void* hder_ptr;

    // set size of user allocation and header pointer
    if (node == NULL) {
        // we haven't found anything, mmap some memory
        avail_space = PAGE_SIZE * div_up(size + HEADER_SIZE, PAGE_SIZE);
        hder_ptr = mmap_wrapper(avail_space);
    } else {
        // we've found a place to put the data
        // remove the node
        free_list_remove(free_head_ptr, node);
        hder_ptr = node;
        avail_space = NODE_SIZE + node->size; 
    }
    
    void* user_ptr = hder_ptr + HEADER_SIZE;
    void* node_ptr = user_ptr + size; // for free node
    
    pthread_t id = pthread_self();

    size_t remainder_size;

    /* vvv CHECK FOR UNDERFLOW vvv */
    if (avail_space < size + NODE_SIZE) {
        remainder_size = 0;
    } else {
        remainder_size = avail_space - (size + HEADER_SIZE);
    }
    /* ^^^ CHECK FOR UNDERFLOW ^^^ */

    if (remainder_size >= NODE_SIZE + size) {
        // we can fit a free node in the remaining space
        create_free_node(node_ptr, remainder_size - NODE_SIZE, id);
        free_list_insert(free_head_ptr, node_ptr);
    } 

    *(alloc_header *) hder_ptr = (alloc_header) {.size = size, .id = id}; 
        
    return user_ptr;
}

void*
hpmalloc(size_t size)
{    
    // if node is too large, put it in its own pages
    if (size + HEADER_SIZE > PAGE_SIZE) {
        void* hder_ptr = mmap(NULL, size + HEADER_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        *(alloc_header *) hder_ptr = (alloc_header) {.size = size, .id = pthread_self()};
        void* user_ptr = hder_ptr + HEADER_SIZE;
        return user_ptr;
    }
    
    int bin = get_bin(size);
    free_node* user_ptr;
    if (bin == NUM_BINS) {
        // global arena
        pthread_mutex_lock(&global_free_list_mutex); // lock the free_list
        free_node* node = global_free_list;
        user_ptr = allocate(&global_free_list, node, size);
        pthread_mutex_unlock(&global_free_list_mutex); // unlock the free_list
    } else {
        // local arena
        size = bin_sizes[bin] - NODE_SIZE; // size that we're going to cut out of our list
        free_node* node = local_bins[bin];        user_ptr = allocate(local_bins + bin, node, size);
    }
    
    return user_ptr;
}

void
hpfree(void* item)
{    
    void* hder_ptr = item - HEADER_SIZE;
    size_t size = ((alloc_header *) hder_ptr)->size;
    
    if (size + HEADER_SIZE > PAGE_SIZE) {
        munmap(hder_ptr, size + HEADER_SIZE);
    } else { 
        // usual case
        int bin = get_bin(size);

        pthread_t node_id = ((alloc_header*) hder_ptr)->id;
        create_free_node(hder_ptr, size, node_id);

        if (bin == NUM_BINS) {
            // global arena
            pthread_mutex_lock(&global_free_list_mutex);
            free_list_insert(&global_free_list, (free_node *) hder_ptr);
            free_list_coalesce(hder_ptr);
            pthread_mutex_unlock(&global_free_list_mutex);
        } else { 
            // fits into a thread local arena
            free_list_insert(local_bins + bin, (free_node *) hder_ptr);
        }
    }
}

void*
hprealloc(void* prev, size_t bytes) {
    size_t size = ((alloc_header *)(prev - HEADER_SIZE))->size;
    size_t copy_size = (bytes < size) ? bytes : size;    
    void* new = hpmalloc(bytes);
    memcpy(new, prev, copy_size);
    hpfree(prev);
    return new;
}