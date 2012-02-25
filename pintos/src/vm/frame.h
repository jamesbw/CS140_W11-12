#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "vm/page.h"

#include "threads/synch.h"


struct hash frame_table;
struct lock frame_table_lock;

unsigned frame_hash (const struct hash_elem *f_, void *aux );
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux );
void *frame_allocate (void *upage);
void frame_free (void *kpage);
void frame_init_base(void *user_base, void *user_end);

struct frame
{
    void *paddr;
    void *upage;
    struct thread *owner_thread;
    bool pinned;
    struct hash_elem elem;
};


#endif
