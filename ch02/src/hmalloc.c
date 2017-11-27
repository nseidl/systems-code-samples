// cs3650 starter code
// Modified by Nicholas Patella
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "hmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

typedef struct free_node {
    size_t size;
    struct free_node* next;
} free_node;

typedef struct alloc_header {
    size_t size;
} alloc_header;

const size_t NODE_SIZE = sizeof(free_node);
const size_t HEADER_SIZE = sizeof(alloc_header);

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.

static free_node* free_head = NULL;
static pthread_mutex_t free_head_mutex = PTHREAD_MUTEX_INITIALIZER;

long
free_list_length()
{
    free_node* current = free_head;
    long size = 0;
    while (current != NULL) {
        current = current->next;
        size++;
    }
    return size;
}

void
free_list_insert(free_node* node) {
    if (free_head == NULL) {
        free_head = node;
    } else {
        free_node* current = free_head;
        free_node* previous = NULL;
        while (current != NULL) {
            if (current > node) {
                break; // we found something
            } else if (current == node) {
                // we have a problem. who did this?
                perror("free_list_insert: tried to insert existing free node");
                exit(1);
            } else {
                previous = current;
                current = current->next;
            }
        }
        
        if (current == NULL) {
            // link at end
            //printf("linked at end\n");
            previous->next = node;
        } else {
            // link before current
            if (previous == NULL) {
                // node becomes new head
                //printf("linked at new head\n");
                free_head = node;
                node->next = current;
            } else {
                // node links normally
                //printf("linked normally\n");
                previous->next = node;
                node->next = current;
            }
        }
    }
}

void free_list_remove(free_node* prev, free_node* this) {
    if (this == free_head) {
        free_head = this->next;
    } else {
        prev->next = this->next;
    }
}

void free_list_coalesce() {
    free_node* current = free_head;
    free_node* previous = NULL;
    while (current != NULL) {
        if (previous != NULL) {
            if ((void*) previous + previous->size + sizeof(size_t) == (void*) current) { // CAST TO VOID POINTER SO IT COUNTS IN BYTES
                // coalesce
                //printf("coal\n");
                previous->next = current->next;
                previous->size += current->size + sizeof(size_t);
                // continue, don't update previous
                current = current->next; continue;
            }
        }
        previous = current;
        current = current->next;
    }
}

static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
void
increment_stat(long* stat, long amt) {
    pthread_mutex_lock(&stats_mutex);
    *stat += amt;
    pthread_mutex_unlock(&stats_mutex);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void*
hmalloc(size_t size)
{
    increment_stat(&stats.chunks_allocated, 1);
    
    // check size of allocation
    if (size + HEADER_SIZE > PAGE_SIZE) {
        // too big, put it in its own pages
        size_t alloc_size = PAGE_SIZE * div_up(size + HEADER_SIZE, PAGE_SIZE);
        void* hder_ptr = mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        increment_stat(&stats.pages_mapped, alloc_size/PAGE_SIZE);
        *(alloc_header *) hder_ptr = (alloc_header) {.size = size};
        void* user_ptr = hder_ptr + HEADER_SIZE;
        return user_ptr;
    }
    
    pthread_mutex_lock(&free_head_mutex); // lock the free_list
    
    // see if we can find a spot to put the data
    free_node* current = free_head;
    free_node* previous = NULL;
    while (current != NULL) {
        if (current->size >= size) {
            break; // we've found it
        } else {
            previous = current;
            current = current->next;
        }
    }
    
    size_t alloc_size;
    void* hder_ptr;
    
    if (current == NULL) {
        // we haven't found anything, mmap some memory
        alloc_size = PAGE_SIZE * div_up(size + HEADER_SIZE, PAGE_SIZE);
        hder_ptr = mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        increment_stat(&stats.pages_mapped, alloc_size/PAGE_SIZE);
    } else {
        // we've found a place to put the data
        // remove the node
        free_list_remove(previous, current);
        hder_ptr = current;
        alloc_size = ((alloc_header *) hder_ptr)->size + HEADER_SIZE;
    }
    
    void* user_ptr = hder_ptr + HEADER_SIZE;
    void* node_ptr = user_ptr + size; // for free node
    
    size_t remainder_size = alloc_size - (size + HEADER_SIZE);
    if (remainder_size >= NODE_SIZE) {
        // we can fit a free node
        *(alloc_header *) hder_ptr = (alloc_header) {.size = size};
        *(free_node *) node_ptr = (free_node) {.size = remainder_size - sizeof(void *), .next = NULL};
        free_list_insert((free_node *) node_ptr);
        free_list_coalesce(); // might coalesce
    } else {
        // we can't fit a free node
        *(alloc_header *) hder_ptr = (alloc_header) {.size = size + remainder_size}; // give the user the useless remaining space
    }
    
    pthread_mutex_unlock(&free_head_mutex); // unlock the free list
        
    return user_ptr;
}

void
hfree(void* item)
{
    increment_stat(&stats.chunks_freed, 1);
    
    void* hder_ptr = item - HEADER_SIZE;
    size_t size = ((alloc_header *) hder_ptr)->size;
    
    if (size + HEADER_SIZE > PAGE_SIZE) {
        // large block special case
        size_t alloc_size = PAGE_SIZE * div_up(size + HEADER_SIZE, PAGE_SIZE);
        munmap(hder_ptr, alloc_size);
        increment_stat(&stats.pages_unmapped, alloc_size/PAGE_SIZE);
    } else { 
        // usual case
        pthread_mutex_lock(&free_head_mutex);
        *(free_node *) hder_ptr = (free_node) {.size = size, .next = NULL};
        free_list_insert((free_node *) hder_ptr);
        pthread_mutex_unlock(&free_head_mutex);
    }
    
    pthread_mutex_lock(&free_head_mutex);
    free_list_coalesce(); // most likely will coalesce
    pthread_mutex_unlock(&free_head_mutex);
}

void*
hrealloc(void* prev, size_t bytes) {
    size_t size = ((alloc_header *)(prev - HEADER_SIZE))->size;
    size_t copy_size = (bytes < size) ? bytes : size;
    void* new = hmalloc(bytes);
    memcpy(new, prev, copy_size);
    hfree(prev);
    return new;
}