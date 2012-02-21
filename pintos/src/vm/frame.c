#include "vm/frame.h"

struct hash frame_table;

/* Returns a hash for frame f */
unsigned frame_hash(const struct hash_elem &f_, void *aux UNUSED) {
  const struct frame *f = hash_entry(f_, struct frame, elem);
  return hash_bytes(&f->addr, sizeof(f->addr));
}

/* Returns true if frame a precedes frame b in physical memory */
bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
  const struct frame *a = hash_entry(a_, struct frame, elem);
  const struct frame *b = hash_entry(b_, struct frame, elem);
  return a->addr < b->addr;
}

hash_init(&frame_table, frame_hash, frame_less, NULL);
