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

// struct hash page_table;

/* Returns a hash for page p */
unsigned 
page_hash(const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry(p_, struct page, elem);
  return hash_bytes(&p->vaddr, sizeof(p->vaddr));
}

/* Returns true if page a precedes page b in virtual memory */
bool 
page_less(const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
  const struct page *a = hash_entry(a_, struct page, elem);
  const struct page *b = hash_entry(b_, struct page, elem);
  return a->vaddr < b->vaddr;
}

// void page_insert_swap (void *vaddr, uint32_t swap_slot)
// {
//   ASSERT ((uint32_t) vaddr % PGSIZE == 0);

//   struct page *new_page = malloc (sizeof (struct page));
//   ASSERT (new_page);

//   new_page->vaddr = vaddr;
//   new_page->type = SWAP;
//   new_page->writable = true;
//   new_page->swap_slot = swap_slot;
//   new_page->mapid = -1;
//   new_page->file = NULL;
//   new_page->offset = -1;
//   new_page->valid_bytes = PGSIZE;

//   lock_acquire (&page_table_lock);
//   hash_insert (&page_table, &new_page->elem);
//   lock_release (&page_table_lock);
// }

struct page * 
page_insert_mmapped (void *vaddr, mapid_t mapid, struct file *file, off_t offset, uint32_t valid_bytes)
{
  ASSERT ((uint32_t) vaddr % PGSIZE == 0);

  struct page *new_page = malloc (sizeof (struct page));
  ASSERT (new_page);

  struct thread *cur = thread_current ();

  new_page->vaddr = vaddr;
  new_page->pd = cur->pagedir;
  new_page->type = MMAPPED;
  new_page->writable = true;
  new_page->swap_slot = -1;
  new_page->mapid = mapid;
  new_page->file = file;
  new_page->offset = offset;
  new_page->valid_bytes = valid_bytes;

  // lock_acquire (&page_table_lock);
  hash_insert (cur->supp_page_table, &new_page->elem);
  // lock_release (&page_table_lock);
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
  new_page->pd = cur->pagedir;
  new_page->type = EXECUTABLE;
  new_page->writable = writable;
  new_page->swap_slot = -1;
  new_page->mapid = -1;
  new_page->file = file;
  new_page->offset = offset;
  new_page->valid_bytes = valid_bytes;

  // lock_acquire (&page_table_lock);
  hash_insert (cur->supp_page_table, &new_page->elem);
  // lock_release (&page_table_lock);

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
  new_page->pd = cur->pagedir;
  new_page->type = ZERO;
  new_page->writable = true;
  new_page->swap_slot = -1;
  new_page->mapid = -1;
  new_page->file = NULL;
  new_page->offset = -1;
  new_page->valid_bytes = 0;

  // lock_acquire (&page_table_lock);
  hash_insert (cur->supp_page_table, &new_page->elem);
  // lock_release (&page_table_lock);

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
  e = hash_find (supp_page_table, &p.elem);
  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

/* Add a zero page to the stack that contains VADDR*/
void page_extend_stack (void *vaddr)
{

// limit stack growth
  int MAX_STACK_SIZE = 8 * 1024 * 1024; // 8MB
  if (PHYS_BASE - vaddr >= MAX_STACK_SIZE)
    thread_exit ();

  void *page_addr = pg_round_down (vaddr);
  void *kpage = frame_allocate (page_addr);
  memset (kpage, 0, PGSIZE);
  pagedir_set_page (thread_current ()->pagedir, page_addr, kpage, true);
  page_insert_zero (page_addr);
}

//  Adds a mapping from user virtual address UPAGE to kernel
//    virtual address KPAGE to the page table.
//    If WRITABLE is true, the user process may modify the page;
//    otherwise, it is read-only.
//    UPAGE must not already be mapped.
//    KPAGE should probably be a page obtained from the user pool
//    with palloc_get_page().
//    Returns true on success, false if UPAGE is already mapped or
//    if memory allocation fails. 
// bool
// install_page (void *upage, void *kpage, bool writable)
// {
//   struct thread *t = thread_current ();

//   /* Verify that there's not already a page at that virtual
//      address, then map our page there. */
//   return (pagedir_get_page (t->pagedir, upage) == NULL
//           && pagedir_set_page (t->pagedir, upage, kpage, writable));
// }



// void page_free_no_delete ( struct hash_elem *elem, void *aux UNUSED)
// {
//   struct page *page = hash_entry (elem, struct page, elem);
//   if (page->type == SWAP)
//   {
//     swap_free (page->swap_slot);
//   }

//   free (page);
// }

void page_free_supp_page_table (void)
{
  hash_destroy (thread_current ()->supp_page_table, page_free_no_delete);
}

void page_free (struct thread *t, void *upage)
{
    struct page p;
    struct hash_elem *e;

    p.vaddr = upage;

    // lock_acquire (&page_table_lock);
    e = hash_delete (t->supp_page_table, &p.elem);
    // lock_release (&page_table_lock);

    ASSERT (e);

    struct page *page = hash_entry (e, struct page, elem);

    if (page->type == SWAP)
    {
      swap_free (page->swap_slot);
    }

    free (page);
}



// void
// page_free (void *upage)
// {
//     struct page p;
//     struct hash_elem *e;

//     p.vaddr = upage;

//     // lock_acquire (&page_table_lock);
//     e = hash_delete (thread_current ()->supp_page_table, &p.elem);
//     // lock_release (&page_table_lock);

//     ASSERT (e);

//     struct page *page = hash_entry (e, struct page, elem);

//     if (page->type == SWAP)
//     {
//       // free swap
//     }

//     free (page);
// }

void page_dump_page ( struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, elem);
  printf ("vaddr: %p\n", page->vaddr);
}

void page_dump_table (void)
{
  hash_apply (thread_current ()->supp_page_table, page_dump_page);
}

bool page_stack_access (void *vaddr, void *esp)
{
  return ( ((uint32_t) vaddr >= (uint32_t) esp )
          || ((uint32_t) vaddr == (uint32_t) esp - 4)
          || ((uint32_t) vaddr == (uint32_t) esp - 32)) ;
}
