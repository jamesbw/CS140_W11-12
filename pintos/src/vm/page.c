#include "page.h"

struct hash page_table;

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

void page_insert_swap (void *vaddr, uint32_t swap_slot)
{
        ASSERT (vaddr % PGSIZE == 0);

        struct page *new_page = malloc (sizeof (struct page));
        ASSERT (new_page);

        new_page->vaddr = vaddr;
        new_page->type = SWAP;
        new_page->writable = true;
        new_page->swap_slot = swap_slot;
        new_page->mapid = -1;
        new_page->file = NULL;
        new_page->offset = -1;
        new_page->valid_bytes = PGSIZE;

        hash_insert (&page_table, &new_page->hash_elem);
}

void page_insert_mmapped (void *vaddr, mapid_t mapid, off_t offset, uint32_t valid_bytes)
{
        ASSERT (vaddr % PGSIZE == 0);

        struct page *new_page = malloc (sizeof (struct page));
        ASSERT (new_page);

        new_page->vaddr = vaddr;
        new_page->type = MMAPPED;
        new_page->writable = true;
        new_page->swap_slot = -1;
        new_page->mapid = mapid;
        new_page->file = NULL;
        new_page->offset = offset;
        new_page->valid_bytes = valid_bytes;

        hash_insert (&page_table, &new_page->hash_elem);
}


void page_insert_executable (void *vaddr, struct file *file, off_t offset, uint32_t valid_bytes, bool writable)
{
        ASSERT (vaddr % PGSIZE == 0);

        struct page *new_page = malloc (sizeof (struct page));
        ASSERT (new_page);

        new_page->vaddr = vaddr;
        new_page->type = EXECUTABLE;
        new_page->writable = writable;
        new_page->swap_slot = -1;
        new_page->mapid = -1;
        new_page->file = file;
        new_page->offset = offset;
        new_page->valid_bytes = valid_bytes;

        hash_insert (&page_table, &new_page->hash_elem);
}


void page_insert_zero (void *vaddr)
{
        ASSERT (vaddr % PGSIZE == 0);

        struct page *new_page = malloc (sizeof (struct page));
        ASSERT (new_page);

        new_page->vaddr = vaddr;
        new_page->type = NONE;
        new_page->writable = true;
        new_page->swap_slot = -1;
        new_page->mapid = -1;
        new_page->file = NULL;
        new_page->offset = -1;
        new_page->valid_bytes = 0;

        hash_insert (&page_table, &new_page->hash_elem);
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (const void *address)
{
  struct page p;
  struct hash_elem *e;

  p.vaddr = address;
  e = hash_find (&page_table, &p.elem);
  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

/* Add a zero page to the stack that contains VADDR*/
void page_extend_stack (void *vaddr)
{
        void *page_addr = pg_round_down (vaddr);
        void *kpage = frame_allocate (page_addr, true);
        memset (kpage, 0, PGSIZE);
        install_page (page_addr, kpage, true);
        page_insert_zero (page_addr);
}

