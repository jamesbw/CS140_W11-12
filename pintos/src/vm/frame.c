#include "vm/frame.h"

struct hash frame_table;

/* Returns a hash for frame f */
unsigned frame_hash(const struct hash_elem &f_, void *aux UNUSED) {
  const struct page *f = hash_entry(f_, struct page, elem);
  return hash_bytes(&f->paddr, sizeof(f->paddr));
}

/* Returns true if frame a precedes frame b in physical memory */
bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
  const struct page *a = hash_entry(a_, struct page, elem);
  const struct page *b = hash_entry(b_, struct page, elem);
  return a->paddr < b->paddr;
}

hash_init(&frame_table, frame_hash, frame_less, NULL);
