#ifndef HPMALLOC_H
#define HPMALLOC_H

// Husky Malloc Interface
// cs3650 Starter Code

typedef struct hpm_stats {
    long pages_mapped;
    long pages_unmapped;
    long chunks_allocated;
    long chunks_freed;
    long free_length;
} hpm_stats;

hpm_stats* hgetstats();
void hprintstats();

void* hpmalloc(size_t size);
void hpfree(void* item);
void* hprealloc(void* prev, size_t bytes);

#endif
