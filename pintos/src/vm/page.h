#ifndef PAGE_H
#define PAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "userprog/process.h"


// struct hash page_table;
// struct lock page_table_lock;

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
  uint32_t *pd;
  enum page_type type;
  bool writable;
  uint32_t swap_slot;
  mapid_t mapid;
  struct file *file;
  off_t offset;
  uint32_t valid_bytes; // mmapped pages might be incomplete and must be filled with zeros.
  struct hash_elem elem;
};


unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
// void page_insert_swap (void *vaddr, uint32_t swap_slot);
struct page *page_insert_mmapped (void *vaddr, mapid_t mapid, struct file *file, off_t offset, uint32_t valid_bytes);
struct page *page_insert_executable (void *vaddr, struct file *file, off_t offset, uint32_t valid_bytes, bool writable);
struct page *page_insert_zero (void *vaddr);
struct page *page_lookup ( void *address);
void page_extend_stack (void *vaddr);
// bool install_page (void *upage, void *kpage, bool writable);
void page_free (void *upage);

void page_dump_page ( struct hash_elem *elem, void *aux UNUSED);
void page_dump_table (void);


#endif
