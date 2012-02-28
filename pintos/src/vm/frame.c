#include "frame.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "page.h"
#include "swap.h"
#include <hash.h>
#include <string.h>
#include <stdio.h>
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

struct hash frame_table;
struct lock frame_table_lock;
void *frame_evict (void);
struct page *run_clock (void);
void *hand;
void *base;
uint32_t user_pool_size;


/* Returns a hash for frame f */
unsigned 
frame_hash (const struct hash_elem *f_, void *aux UNUSED) {
    const struct page *p = hash_entry(f_, struct page, frame_elem);
    return hash_bytes(&p->paddr, sizeof(p->paddr));
}

/* Returns true if frame a precedes frame b in physical memory */
bool 
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct page *a = hash_entry(a_, struct page, frame_elem);
    const struct page *b = hash_entry(b_, struct page, frame_elem);
    return a->paddr < b->paddr;
}


void
frame_allocate (struct page *page)
{
    ASSERT (pagedir_get_page (page->pd, page->vaddr) == NULL );
    ASSERT (page->paddr == NULL);

    void *kpage = palloc_get_page (PAL_USER);
    if (kpage == NULL)
    {
        //TODO: eviction
        kpage = frame_evict();
    }

    page->paddr = kpage;
    page->pinned = true;
    lock_acquire (&frame_table_lock);
    hash_insert (&frame_table, &page->frame_elem);
    lock_release (&frame_table_lock);
}


void *
frame_evict (void)
{
    lock_acquire (&frame_table_lock);
    struct page *page_to_evict;

    page_to_evict = run_clock ();

    ASSERT (page_to_evict->paddr);
    ASSERT (pagedir_get_page (page_to_evict->pd, page_to_evict->vaddr));

    void *kpage = page_to_evict->paddr;
    lock_acquire (&page_to_evict->busy);
    pagedir_clear_page (page_to_evict->pd, page_to_evict->vaddr);

    switch (page_to_evict->type)
    {
        case EXECUTABLE:
        case ZERO:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //move to swap
                page_to_evict->type = SWAP;
                page_to_evict->swap_slot = swap_allocate_slot ();
                lock_acquire (&filesys_lock);
                swap_write_page ( page_to_evict->swap_slot, page_to_evict->paddr);
                lock_release (&filesys_lock);
            }
            break;
        case SWAP:
            page_to_evict->swap_slot = swap_allocate_slot ();
            lock_acquire (&filesys_lock);
            swap_write_page ( page_to_evict->swap_slot, page_to_evict->paddr);
            lock_release (&filesys_lock);
            break;
        case MMAPPED:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //copy back to disk
                lock_acquire (&filesys_lock);
                file_seek (page_to_evict->file, page_to_evict->offset);
                file_write (page_to_evict->file, page_to_evict->paddr, page_to_evict->valid_bytes);
                lock_release (&filesys_lock);
            }
            pagedir_set_dirty (page_to_evict->pd, page_to_evict->vaddr, false); 
            break;            
    }


    hash_delete (&frame_table, &page_to_evict->frame_elem);
    page_to_evict->paddr = NULL;
    lock_release (&page_to_evict->busy);


    lock_release (&frame_table_lock);
    return kpage;

}

void 
frame_pin (void *vaddr)
{
    lock_acquire (&frame_table_lock);
    void *upage = pg_round_down (vaddr);
    struct page *supp_page = page_lookup (thread_current ()->supp_page_table, upage);

    supp_page->pinned = true;
    lock_release (&frame_table_lock);

    if (supp_page->paddr == NULL)
    {
        page_in (supp_page);
    }
}

void 
frame_unpin (void *vaddr)
{
    void *upage = pg_round_down (vaddr);
    struct page *supp_page = page_lookup (thread_current ()->supp_page_table, upage);

    ASSERT (supp_page->paddr);

    supp_page->pinned = false;
}

void 
frame_dump_frame ( struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, frame_elem);
  printf ("paddr: %p , vaddr: %p\n", page->paddr, page->vaddr);
}

void 
frame_dump_table (void)
{
  lock_acquire (&frame_table_lock);
  hash_apply (&frame_table, frame_dump_frame);
  lock_release (&frame_table_lock);
}


void 
frame_init_base (void *user_base, void *user_end) 
{
  base = user_base;
  user_pool_size = (uint32_t)(user_end - user_base);
  hand = user_base;
}

/* Returns pointer to the physical frame that should be written to next.
   This algorithm approximates a LRU heuristic.  Only checks user pages,
   as kernal pages should never be evicted. This frame pointer is then
   passed to the eviction function. */
struct page *
run_clock (void) 
{
  struct page p;  // Dummy page for hash_find comparison.
  struct page *page_to_evict; // Pointer to the actual page.
  struct hash_elem *e;
  while (true) {
    //advance hand:
    hand = (uint32_t) (hand + PGSIZE - base) % user_pool_size + base;
    p.paddr = hand;
    e = hash_find(&frame_table, &p.frame_elem);
    if (e != NULL) 
    {
      page_to_evict = hash_entry(e, struct page, frame_elem);
      if (!pagedir_is_accessed(page_to_evict->pd, page_to_evict->vaddr))
      { 
        if (page_to_evict->pinned == false)
          return page_to_evict;
      }
      else 
        pagedir_set_accessed(page_to_evict->pd, page_to_evict->vaddr, false);
    }
  }
}
