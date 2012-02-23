#ifndef PAGE_H
#define PAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "filesys/off_t.h"

typedef int mapid_t;


struct hash page_table;
struct lock page_table_lock;

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void page_insert_swap (void *vaddr, uint32_t swap_slot);
void page_insert_mmapped (void *vaddr, mapid_t mapid, off_t offset, uint32_t valid_bytes);
void page_insert_executable (void *vaddr, struct file *file, off_t offset, uint32_t valid_bytes, bool writable);
void page_insert_zero (void *vaddr);
page_lookup (const void *address);
void page_extend_stack (void *vaddr);



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
