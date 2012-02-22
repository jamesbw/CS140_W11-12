#ifndef PAGE_H
#define PAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>

/* This struct will be the primary unit used to manage memory.  Pages will
   be put into hash tables according to one or another field, e.g. the
   virtual address + page directory for the supplemental page table, or
   the physical address for the frame table.
   
   NOTE:
   Other fields will need to be added to support mmaped files or other
   information as we come to it. 
*/

struct page {
  struct hash_elem elem;
  void *paddr;
  void *vaddr;
  uint32_t *pd;
  int IN_SWAP:1; /*single bit, should consider combining into some flag
		   with defined masks */
  int MMAPPED:1; /*single bit, as above */ 
};



#endif
