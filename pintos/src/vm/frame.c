#include "frame.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "page.h"
#include <hash.h>

struct hash frame_table;

/* Returns a hash for frame f */
unsigned 
frame_hash (const struct hash_elem *f_, void *aux UNUSED) {
    const struct frame *f = hash_entry(f_, struct frame, elem);
    return hash_bytes(&f->paddr, sizeof(f->paddr));
}

/* Returns true if frame a precedes frame b in physical memory */
bool 
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct frame *a = hash_entry(a_, struct frame, elem);
    const struct frame *b = hash_entry(b_, struct frame, elem);
    return a->paddr < b->paddr;
}


void *
frame_allocate (void *upage)
{
    ASSERT (pagedir_get_page (thread_current ()->pagedir, upage) == NULL );

    void *kpage = palloc_get_page (PAL_USER);
    ASSERT (kpage);
    if (kpage == NULL)
    {
        //TODO: eviction
        kpage = frame_evict_and_reallocate (upage);
    }

    // add frame to frame table
    struct frame *new_frame = malloc (sizeof (struct frame));
    ASSERT (new_frame);

    new_frame->paddr = kpage; // TODO - PHYS_BASE?
    new_frame->owner_thread = thread_current ();
    new_frame->upage = upage;
    new_frame->pinned = false;


    hash_insert (&frame_table, &new_frame->elem);
    return kpage;
}

void
frame_free (void *kpage)
{
    struct frame f;
    struct hash_elem *e;

    f.paddr = kpage;
    e = hash_delete (&frame_table, &f.elem);

    ASSERT (e);

    palloc_free_page (kpage);

    free (hash_entry (e, struct frame, elem));
}

void *
frame_evict_and_reallocate (void *upage)
{
    struct hash_iterator it;
    hash_first (&it, &frame_table);

    do
    {
        struct frame *frame_to_evict = hash_entry (hash_next (&it), struct frame, elem);
    }
    while (frame_to_evict->pinned == true);

    struct page *page_to_evict = page_lookup(); //TODO




}
