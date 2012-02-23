#include "vm/page.h"

/* Preliminary page_hash function.  Because each process has its own
   virtual address space, I try to hash based on both the vaddr and the
   pd, but this could be a problem if the struct is rearranged in
   memory. */
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry(p_, struct page, elem);
  return hash_bytes(&p->vaddr, sizeof(p->vaddr)+sizeof(p->pd));
}


/* Preliminary hash_less function.  Because each process has its own
   virtual address, sorting by vaddr may not be correct.  Perhaps
   switching to the kernal virtual address or some other value will be
   better */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_,
	       void *aux UNUSED) {
  const struct page *a = hash_entry(a_, struct page, elem);
  const struct page *b = hash_entry(b_, struct page, elem);
  return a->vaddr < b->vaddr;
}

struct hash supp_table;


hash_init (&supp_table, page_hash, page_less, NULL);
