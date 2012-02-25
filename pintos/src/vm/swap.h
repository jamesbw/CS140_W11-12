#ifndef SWAP_H
#define SWAP_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include <kernel/bitmap.h>


struct bitmap *swap_bitmap;

void swap_init (void);
void swap_free (uint32_t swap_slot);
uint32_t swap_allocate_slot (void);
void swap_read_page (uint32_t swap_slot, void *buf);
void swap_write_page (uint32_t swap_slot, void *buf);


#endif
