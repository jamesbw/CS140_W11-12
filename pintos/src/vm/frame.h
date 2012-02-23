#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>


struct hash frame_table;
struct lock frame_table_lock;

unsigned frame_hash(const struct hash_elem &f_, void *aux UNUSED);
bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

struct frame
{
	void *paddr;
	void *upage;
	bool pinned;
	hash_elem elem;
};



#endif