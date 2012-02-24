#include "frame.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"

struct hash frame_table;
void *clock_start;// = user_pool->base;
/* Returns a hash for frame f */


void *run_clock(void);

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
    // uint32_t *pd = thread_current ()->pagedir;
    void *kpage = palloc_get_page (PAL_USER);
    ASSERT (kpage);
    if (kpage == NULL)
    {
        //TODO: eviction
      kpage = run_clock();
    }

    // if (!install_page (upage, kpage, writable))
    // {
    //     palloc_free_page (kpage);
    //     return NULL;
    // }

    // add frame to frame table
    struct page *new_frame = malloc (sizeof (struct page));
    ASSERT (new_frame);

    new_frame->paddr = kpage; // TODO - PHYS_BASE?
    new_frame->upage = upage;
    new_frame->pinned = false;


    hash_insert (&frame_table, &new_frame->elem);
    return kpage;
}

void
frame_free (void *kpage)
{
  lock_acquire(&frame_table_lock);
  struct frame f;
  struct hash_elem *e;
  
  f.paddr = kpage;
  e = hash_delete (&frame_table, &f.elem);
  
  ASSERT (e);
  
  free (hash_entry (e, struct page, elem));
  lock_release(&frame_table);
}


void *run_clock() {
  lock_acquire(&frame_table_lock);
  void *hand = clock_start;
  struct page p;  // Dummy page for hash_find comparison.
  struct page *f; // Pointer to the actual frame.
  struct hash_elem *e;
  struct thread *t = thread_current();
  uint32_t *pd = t->pagedir; // Current Page Directory
  while (1) {
    p.paddr = hand;
    e = hash_find(&frame_table, &p.elem);
    if (e != NULL) {
      f = hash_entry(e, struct page, elem);
      if (!pagedir_is_accessed(pd, hand)) {
	if (!pagedir_is_dirty(pd, hand)) {
	  clock_start = ((uint32_t)hand + 4)% (uint32_t)(user_pool->base);
	  lock_release(&frame_table_lock);
	  return hand;
	} else { /* Is dirty */
	  //page_dir_set_dirty(pd, hand, false);
	  /* Begin writing to disk */
	}
      } else {
	pagedir_set_accessed(pd, hand, false);
      }
    } else {
      clock_start = ((uint32_t)hand + 4) % (uint32_t)(user_pool->base);
      lock_release(&frame_table_lock);
      return hand;
    }
    hand = ((uint32_t)hand + 4) % (uint32_t)(user_pool->base);
    if (hand == clock_start) {
      lock_release(&frame_table_lock);
      return NULL;
    }
  }
}
