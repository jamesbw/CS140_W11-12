#include "page.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "frame.h"
#include "swap.h"
#include "userprog/pagedir.h"
#include <string.h>
#include <stdio.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "sharing.h"

// void page_free_no_delete ( struct hash_elem *elem, void *aux UNUSED);


/* Returns a hash for page p */
unsigned 
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry(p_, struct page, page_elem);
  return hash_bytes(&p->vaddr, sizeof(p->vaddr));
}

/* Returns true if page a precedes page b in virtual memory */
bool 
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
  const struct page *a = hash_entry(a_, struct page, page_elem);
  const struct page *b = hash_entry(b_, struct page, page_elem);
  return a->vaddr < b->vaddr;
}


struct page * 
page_insert_mmapped (void *vaddr, mapid_t mapid, struct file *file, off_t offset, uint32_t valid_bytes)
{
  ASSERT ((uint32_t) vaddr % PGSIZE == 0);

  struct page *new_page = malloc (sizeof (struct page));
  ASSERT (new_page);

  struct thread *cur = thread_current ();

  new_page->vaddr = vaddr;
  new_page->paddr = NULL;
  new_page->pinned = false;
  new_page->pd = cur->pagedir;
  new_page->type = MMAPPED;
  new_page->writable = true;
  new_page->swap_slot = -1;
  new_page->mapid = mapid;
  new_page->file = file;
  new_page->offset = offset;
  new_page->valid_bytes = valid_bytes;
  lock_init(&new_page->busy);

  hash_insert (cur->supp_page_table, &new_page->page_elem);
  return new_page;
}


struct page * 
page_insert_executable (void *vaddr, struct file *file, off_t offset, uint32_t valid_bytes, bool writable)
{
  ASSERT ((uint32_t) vaddr % PGSIZE == 0);

  struct page *new_page = malloc (sizeof (struct page));
  ASSERT (new_page);

  struct thread *cur = thread_current ();

  new_page->vaddr = vaddr;
  new_page->paddr = NULL;
  new_page->pinned = false;
  new_page->pd = cur->pagedir;
  new_page->type = EXECUTABLE;
  new_page->writable = writable;
  new_page->swap_slot = -1;
  new_page->mapid = -1;
  new_page->file = file;
  new_page->offset = offset;
  new_page->valid_bytes = valid_bytes;
  lock_init(&new_page->busy);

  if (writable == false)
    sharing_register_page (new_page);

  hash_insert (cur->supp_page_table, &new_page->page_elem);

  return new_page;
}


struct page * 
page_insert_zero (void *vaddr)
{
  ASSERT ((uint32_t) vaddr % PGSIZE == 0);

  struct page *new_page = malloc (sizeof (struct page));
  ASSERT (new_page);

  struct thread *cur = thread_current ();

  new_page->vaddr = vaddr;
  new_page->paddr = NULL;
  new_page->pinned = false;
  new_page->pd = cur->pagedir;
  new_page->type = ZERO;
  new_page->writable = true;
  new_page->swap_slot = -1;
  new_page->mapid = -1;
  new_page->file = NULL;
  new_page->offset = -1;
  new_page->valid_bytes = 0;
  lock_init(&new_page->busy);

  hash_insert (cur->supp_page_table, &new_page->page_elem);

  return new_page;
}

/* Returns the struct page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (struct hash *supp_page_table, void *address)
{
  struct page p;
  struct hash_elem *e;

  p.vaddr = address;
  e = hash_find (supp_page_table, &p.page_elem);
  return e != NULL ? hash_entry (e, struct page, page_elem) : NULL;
}

/* Add a zero page to the stack that contains VADDR*/
void 
page_extend_stack (void *vaddr)
{

// limit stack growth
  int MAX_STACK_SIZE = 8 * 1024 * 1024; // 8MB
  if (PHYS_BASE - vaddr >= MAX_STACK_SIZE)
    thread_exit ();

  void *page_addr = pg_round_down (vaddr);
  struct page *new_page = page_insert_zero (page_addr);

  page_in (new_page);
  new_page->pinned = false;
}

void 
page_free ( struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, page_elem);

  if (page->type == EXECUTABLE && page->writable == false)
  {
    sharing_unregister_page (page);
  }

  if (page->paddr){
    lock_acquire (&frame_table_lock);
    if (page->paddr)
    {
      hash_delete (&frame_table, &page->frame_elem);
      pagedir_clear_page (page->pd, page->vaddr);
      palloc_free_page (page->paddr);
      page->paddr = NULL;
    }
    lock_release (&frame_table_lock);
  }

  lock_acquire (&page_to_evict->busy);
  if (page->type == SWAP)
  {
    if ( (int) page->swap_slot != -1)
        swap_free (page->swap_slot);
  }
  lock_release (&page_to_evict->busy);
  free(page);
}

void 
page_free_supp_page_table (void)
{
  hash_destroy (thread_current ()->supp_page_table, page_free);
}


void
page_in (struct page *supp_page)
{

  if (supp_page->type == EXECUTABLE && supp_page->writable == false)
  {
    void *shared_paddr = sharing_find_shared_frame (supp_page);
    if (shared_paddr)
    {
      supp_page->paddr = shared_paddr;
      // lock_release (&supp_page->busy);
      pagedir_set_page (supp_page->pd, supp_page->vaddr, supp_page->paddr, supp_page->writable);
      return;
    }
  }


  lock_acquire (&supp_page->busy);
  frame_allocate (supp_page);
  switch (supp_page->type)
  {
    case EXECUTABLE:
    case MMAPPED:
      lock_acquire (&filesys_lock);
      file_seek (supp_page->file, supp_page->offset);
      file_read (supp_page->file, supp_page->paddr, supp_page->valid_bytes);
      lock_release (&filesys_lock);
      memset (supp_page->paddr + supp_page->valid_bytes, 0, PGSIZE - supp_page->valid_bytes);
      break;
    case SWAP:
      lock_acquire (&filesys_lock);
      swap_read_page (supp_page->swap_slot, supp_page->paddr);
      lock_release (&filesys_lock);
      swap_free (supp_page->swap_slot);
      supp_page->swap_slot = -1;
      break;
    case ZERO:
      memset (supp_page->paddr, 0, PGSIZE);
      break;
    default:
      break;
  }
  lock_release (&supp_page->busy);
  pagedir_set_page (supp_page->pd, supp_page->vaddr, supp_page->paddr, supp_page->writable);
}


void 
page_dump_page ( struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, page_elem);
  printf ("vaddr: %p\n", page->vaddr);
}

void 
page_dump_table (void)
{
  hash_apply (thread_current ()->supp_page_table, page_dump_page);
}

bool
page_stack_access (void *vaddr, void *esp)
{
  return ( ((uint32_t) vaddr >= (uint32_t) esp )
          || ((uint32_t) vaddr == (uint32_t) esp - 4)
          || ((uint32_t) vaddr == (uint32_t) esp - 32)) ;
}
