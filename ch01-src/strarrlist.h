// strarrlist.h
// This code is from Nat Tuck's lecture notes 07-io-and-shell
// http://www.ccs.neu.edu/home/ntuck/courses/2017/09/cs3650/notes/07-io-and-shell/calc/
// strarrlist_contains and strarrlist_delete_first were written by myself (Nick Seidl)

#ifndef strarrlist_H
#define strarrlist_H

// c implementation of an Arraylist that deals with Strings
typedef struct strarrlist {
	// fields that a strarrlist has
	int cap; // 8
    int size; // 4
    char** data; // 8
} strarrlist;

// a way to allocate memory for this svec
strarrlist* make_strarrlist();

// a way to free memory for this svec
void  free_strarrlist(strarrlist* sal);

// get a string from this svec
char* strarrlist_get(strarrlist* sal, int i);

// put a string into some location in this svec
void strarrlist_set(strarrlist* sal, int i, char* item);

void strarrlist_add(strarrlist* sal, char* item);

void strarrlist_sort(strarrlist* sal);

// returns -1 if not found; else, return index
int strarrlist_contains(strarrlist* sal, char* item);

// deletes a string
void strarrlist_delete_first(strarrlist* sal);

#endif