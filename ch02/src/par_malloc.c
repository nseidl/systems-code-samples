#include <stdlib.h>
#include <unistd.h>

#include "xmalloc.h"
#include "hpmalloc.h"

void*
xmalloc(size_t bytes)
{
    return hpmalloc(bytes);
}

void
xfree(void* ptr)
{
    hpfree(ptr);
}

void*
xrealloc(void* prev, size_t bytes)
{
    return hprealloc(prev, bytes);
}

