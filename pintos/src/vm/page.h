#ifndef PAGE_H
#define PAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>


struct hash page_table;
struct lock page_table_lock;

unsigned page_hash(const struct hash_elem &p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

enum page_type
{
  SWAP,
  MMAPPED,
  EXECUTABLE,
  NONE,
};


struct page 
{
  void *vaddr;
  enum page_type type;
  bool writable;
  uint32_t swap_slot;
  mapid_t mapid;
  struct file *file;
  off_t offset;
  uint32_t valid_bytes; // mmapped pages might be incomplete and must be filled with zeros.
  struct hash_elem elem;
};



#endif