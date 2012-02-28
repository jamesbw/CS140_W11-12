#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "vm/page.h"

#include "threads/synch.h"
#include "page.h"


struct hash frame_table;
struct lock frame_table_lock;

unsigned frame_hash (const struct hash_elem *f_, void *aux );
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux );

void frame_allocate (struct page *page);
void frame_pin (void *vaddr);
void frame_unpin (void *vaddr);
void frame_dump_frame ( struct hash_elem *elem, void *aux UNUSED);
void frame_dump_table (void);
void frame_init_base(void *user_base, void *user_end);



#endif
