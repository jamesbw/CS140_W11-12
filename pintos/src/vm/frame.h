#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "threads/synch.h"


struct hash frame_table;
struct lock frame_table_lock;

unsigned frame_hash (const struct hash_elem *f_, void *aux UNUSED);
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void *frame_allocate (void *upage, bool writable);
void frame_free (void *kpage);

struct frame
{
    void *paddr;
    void *upage;
    bool pinned;
    struct hash_elem elem;
};



#endif
