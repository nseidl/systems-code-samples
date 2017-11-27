// strarrlist.c
// This code is from Nat Tuck's lecture notes 07-io-and-shell
// http://www.ccs.neu.edu/home/ntuck/courses/2017/09/cs3650/notes/07-io-and-shell/calc/
// strarrlist_contains and strarrlist_delete_first were written by myself (Nick Seidl)

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "strarrlist.h"

strarrlist*
make_strarrlist()
{
    strarrlist* sal = malloc(sizeof(strarrlist));
    sal->size = 0; // (*sal).size = 0;
    sal->cap  = 4;
    sal->data = malloc(4 * sizeof(char*));
    memset(sal->data, 0, 4 * sizeof(char*));
    return sal;
}

void
free_strarrlist(strarrlist* sal)
{
    int i;
    for (i = 0; i < sal->size; i++) {
        if (sal->data[i] != 0) {
            free(sal->data[i]);
        }
    }

    free(sal->data);
    free(sal);
}

char*
strarrlist_get(strarrlist* sal, int i)
{
    assert(i >= 0 && i < sal->size);
    return sal->data[i];
}

void
strarrlist_set(strarrlist* sal, int i, char* item)
{
    assert(i >= 0 && i < sal->size);
    //printf("i is <%i> and item is <%s>\n", i, item);
    free(sal->data[i]);
    sal->data[i] = strdup(item); // copies a string into newly mallocd space
}

void 
strarrlist_add(strarrlist* sal, char* item)
{
    int i = sal->size;

    if (i >= sal->cap) {
        sal->cap *= 2;
        sal->data = (char**)realloc(sal->data, sal->cap * sizeof(char*));
        int q;
        for (q = i; q < sal->cap; q++)
        {
            sal->data[q] = 0;
        }
    }

    sal->size = i + 1;
    strarrlist_set(sal, i, item);
}

// returns -1 if not found; else, return index
int 
strarrlist_contains(strarrlist* sal, char* item)
{
    int i;
    for (i = 0; i < sal->size; i++)
    {
        if (strcmp(item, sal->data[i]) == 0)
        {
            return i;
        }
    }

    return -1;
}

void
strarrlist_delete_first(strarrlist* sal)
{
    assert(sal->size > 0);
    int q;
    for (q = 0; q < sal->size - 1; q++)
    {
        strarrlist_set(sal, q, strarrlist_get(sal, q+1));
    }

    free(strarrlist_get(sal, sal->size - 1));

    sal->size = sal->size - 1;
}


