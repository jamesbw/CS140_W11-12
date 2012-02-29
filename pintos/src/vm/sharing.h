#ifndef VM_SHARING_H
#define VM_SHARING_H

#include <hash.h>
#include <list.h>

struct hash executable_table;

struct shared_executable
{
	block;
	offset;
	struct hash_elem elem;
};

void sharing_clear_accessed_bit ();
void sharing_get_accessed_bit ();

#endif
