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

struct hash frame_table;
void *clock_start;
void *base;
void *end;
struct frame *frame_evict (void);
void *run_clock(void);

void frame_init_base(void *user_base, void *user_end) {
  base = user_base;
  clock_start = user_base;
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
    struct frame *new_frame;
    if (kpage == NULL)
    {
        //TODO: eviction
      printf("PAGEFAULT\n");
        new_frame = frame_evict();
    }
    else{
        
        new_frame = malloc (sizeof (struct frame));
        ASSERT (new_frame);
        new_frame->pinned = true;
        new_frame->paddr = kpage; // TODO - PHYS_BASE?
        lock_acquire (&frame_table_lock);
        hash_insert (&frame_table, &new_frame->elem);
        lock_release (&frame_table_lock);
    }
    // add frame to frame table

    new_frame->owner_thread = thread_current ();
    new_frame->upage = upage;

    return new_frame->paddr;
}


void
frame_free (void *kpage)
{
    struct frame f;
    // struct hash_elem *e;

    struct frame *frame_to_delete;

    f.paddr = kpage;
    lock_acquire (&frame_table_lock);
    frame_to_delete = hash_entry (hash_find (&frame_table, &f.elem), struct frame, elem);
    if (frame_to_delete->owner_thread == thread_current ())
    {
        hash_delete (&frame_table, &f.elem);
        free (frame_to_delete);
        palloc_free_page (kpage);
    }

    lock_release (&frame_table_lock);

    // ASSERT (e);


    // free (hash_entry (e, struct frame, elem));
}

struct frame*
frame_evict (void)
{
    lock_acquire (&frame_table_lock);
    struct frame p;
    struct hash_elem *e;
    struct frame *frame_to_evict;
    p.paddr = run_clock();
    ASSERT(p.paddr);
    e = hash_find(&frame_table, &(p.elem));
    ASSERT(e);
    frame_to_evict = hash_entry(e, struct frame, elem);
    frame_to_evict->pinned = true;
    struct page *page_to_evict = page_lookup(frame_to_evict->owner_thread->supp_page_table, frame_to_evict->upage);
    frame_to_evict->owner_thread = NULL;
    ASSERT (pagedir_get_page (page_to_evict->pd, page_to_evict->vaddr));
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
                swap_write_page ( page_to_evict->swap_slot, frame_to_evict->paddr);
                lock_release (&filesys_lock);
            }
            break;
        case SWAP:
            page_to_evict->swap_slot = swap_allocate_slot ();
            lock_acquire (&filesys_lock);
            swap_write_page ( page_to_evict->swap_slot, frame_to_evict->paddr);
            lock_release (&filesys_lock);
            break;
        case MMAPPED:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //copy back to disk
                lock_acquire (&filesys_lock);
                file_seek (page_to_evict->file, page_to_evict->offset);
                file_write (page_to_evict->file, frame_to_evict->paddr, page_to_evict->valid_bytes);
                lock_release (&filesys_lock);
            }
            break;            
    }


    // void *result_kpage = frame_to_evict->paddr;
    lock_release (&frame_table_lock);
    lock_release (&page_to_evict->busy);
    // frame_free (result_kpage);
    return frame_to_evict;

}


/* Returns pointer to the physical frame that should be written to next.
   This algorithm approximates a LRU heuristic.  Only checks user pages,
   as kernal pages should never be evicted. This frame pointer is then
   passed to the eviction function. */
void *run_clock() {
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
      if (!pagedir_is_accessed(f->owner_thread->pagedir, f->upage)) {
	  clock_start = ((uint32_t)hand + 4)% pool_size + base;
	  return hand;
      } else {
	pagedir_set_accessed(f->owner_thread->pagedir, f->upage, false);
      }
    } else {
      clock_start = ((uint32_t)hand + 4)% pool_size + base;
      return hand;
    }
    hand = ((uint32_t)hand + 4)% pool_size + base;
    if (hand == clock_start) {
      return NULL;
    }
  }
}


struct frame *
frame_lookup (void *paddr)
{
    struct frame f;
    f.paddr = paddr;
    struct hash_elem *e;
    if (!lock_held_by_current_thread (&frame_table_lock))
    {
        lock_acquire (&frame_table_lock);
        e = hash_find (&frame_table, &f.elem);
        lock_release (&frame_table_lock);
    }
    else
        e = hash_find (&frame_table, &f.elem);
    
    return e!= NULL ? hash_entry (e, struct frame, elem) : NULL; 
}

void 
frame_pin (void *vaddr)
{
    lock_acquire (&frame_table_lock);
    void *upage = pg_round_down (vaddr);
    struct page *supp_page = page_lookup (thread_current ()->supp_page_table, upage);

    void *kpage = pagedir_get_page (supp_page->pd, upage);

    if (kpage == NULL)
    {
        lock_release (&frame_table_lock);
        kpage = frame_allocate (upage);
        lock_acquire (&supp_page->busy);
        switch (supp_page->type)
        {
          case EXECUTABLE:
          case MMAPPED:
            lock_acquire (&filesys_lock);
            file_seek (supp_page->file, supp_page->offset);
            file_read (supp_page->file, kpage, supp_page->valid_bytes);
            lock_release (&filesys_lock);
            memset (kpage + supp_page->valid_bytes, 0, PGSIZE - supp_page->valid_bytes);
            break;
          case SWAP:
            lock_acquire (&filesys_lock);
            swap_read_page (supp_page->swap_slot, kpage);
            lock_release (&filesys_lock);
            swap_free (supp_page->swap_slot);
            supp_page->swap_slot = -1;
            break;
          case ZERO:
            memset (kpage, 0, PGSIZE);
            break;
          default:
            break;
        }
        lock_release (&supp_page->busy);
        pagedir_set_page (supp_page->pd, upage, kpage, supp_page->writable);
    }
    else
    {
        frame_lookup (kpage)->pinned = true;
        lock_release (&frame_table_lock);
    }
}

void 
frame_unpin (void *vaddr)
{
    void *upage = pg_round_down (vaddr);
    void *kpage = pagedir_get_page (thread_current ()->pagedir, upage);

    ASSERT (kpage);

    frame_lookup (kpage)->pinned = false;
}

void frame_dump_frame ( struct hash_elem *elem, void *aux UNUSED)
{
  struct frame *frame = hash_entry (elem, struct frame, elem);
  printf ("paddr: %p , upage: %p\n", frame->paddr, frame->upage);
}

void frame_dump_table (void)
{
  lock_acquire (&frame_table_lock);
  hash_apply (&frame_table, frame_dump_frame);
  lock_release (&frame_table_lock);
}

