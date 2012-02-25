#include "frame.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

struct hash frame_table;
void *clock_start;
void *base;
void *end;



void *run_clock(void);

void frame_init_base(void *user_base, void *user_end) {
  clock_start = base;
  base = user_base;
  end = user_end;
}

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
    //ASSERT (kpage);
    if (kpage == NULL)
    {
        //TODO: eviction
      kpage = run_clock();
    }

    // add frame to frame table
    struct frame *new_frame = malloc (sizeof (struct frame));
    ASSERT (new_frame);

    new_frame->paddr = kpage; // TODO - PHYS_BASE?
    new_frame->owner_thread = thread_current ();
    new_frame->upage = upage;
    new_frame->pinned = false;
    
    lock_acquire(&frame_table_lock);
    hash_insert (&frame_table, &new_frame->elem);
    lock_release(&frame_table_lock);
    return kpage;
}

void
frame_free (void *kpage)
{
  struct frame f;
  struct hash_elem *e;
  
  f.paddr = kpage;
  lock_acquire(&frame_table_lock);
  e = hash_delete (&frame_table, &f.elem);
  
  ASSERT (e);

  free (hash_entry (e, struct frame, elem));
  lock_release(&frame_table_lock);
}

/* Returns pointer to the physical frame that should be written to next.
   This algorithm approximates a LRU heuristic.  Only checks user pages,
   as kernal pages should never be evicted. */
void *run_clock() {
  lock_acquire(&frame_table_lock);
  void *hand = clock_start;
  uint32_t pool_size = (uint32_t)end - (uint32_t)base;
  struct frame p;  // Dummy page for hash_find comparison.
  struct frame *f; // Pointer to the actual frame.
  struct hash_elem *e;
  struct thread *t = thread_current();
  uint32_t *pd = t->pagedir; // Current Page Directory
  while (1) {
    p.paddr = hand;
    e = hash_find(&frame_table, &p.elem);
    if (e != NULL) {
      f = hash_entry(e, struct frame, elem);
      if (!pagedir_is_accessed(pd, hand)) {
	if (!pagedir_is_dirty(pd, hand)) {
	  clock_start = ((uint32_t)hand + 4)% pool_size + base;
	  lock_release(&frame_table_lock);
	  return hand;
	} else { /* Is dirty */
	  uint32_t slot = swap_allocate_slot();
	  pagedir_set_dirty(pd, hand, false);
	  swap_write_page(slot, f->upage);
	  /* Begin writing to disk */
	}
      } else {
	pagedir_set_accessed(pd, hand, false);
      }
    } else {
      clock_start = ((uint32_t)hand + 4)% pool_size + base;
      lock_release(&frame_table_lock);
      return hand;
    }
    hand = ((uint32_t)hand + 4)% pool_size + base;
    if (hand == clock_start) {
      lock_release(&frame_table_lock);
      return NULL;
    }
  }
}

void *
frame_evict (void)
{
    struct hash_iterator it;
    hash_first (&it, &frame_table);
    struct frame *frame_to_evict;

    do
    {
        frame_to_evict = hash_entry (hash_next (&it), struct frame, elem);
    }
    while (frame_to_evict->pinned == true);

    struct page *page_to_evict = page_lookup(frame_to_evict->owner_thread->supp_page_table, frame_to_evict->upage);

    switch (page_to_evict->type)
    {
        case EXECUTABLE:
        case ZERO:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //move to swap
                page_to_evict->type = SWAP;
                page_to_evict->swap_slot = swap_allocate_slot ();
                swap_write_page ( page_to_evict->swap_slot, page_to_evict->vaddr);
            }
            break;
        case SWAP:
            page_to_evict->swap_slot = swap_allocate_slot ();
            swap_write_page ( page_to_evict->swap_slot, page_to_evict->vaddr);
            break;
        case MMAPPED:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //copy back to disk
                file_seek (page_to_evict->file, page_to_evict->offset);
                file_write (page_to_evict->file, page_to_evict->vaddr, page_to_evict->valid_bytes);
            }
            break;            
    }

    pagedir_clear_page (page_to_evict->pd, page_to_evict->vaddr);
    return frame_to_evict->paddr;

}
