#include "swap.h"
#include "devices/block.h"
#include <kernel/bitmap.h>
#include "threads/vaddr.h"
#include "threads/synch.h"


void swap_init (void)
{
	struct block *swap_block = block_get_role (BLOCK_SWAP);
	swap_bitmap = bitmap_create (block_size (swap_block) * BLOCK_SECTOR_SIZE / PGSIZE);
	lock_init (&swap_lock);
}

void swap_free (uint32_t swap_slot)
{
	bitmap_reset (swap_bitmap, swap_slot);
}

uint32_t swap_allocate_slot (void)
{	
	lock_acquire (&swap_lock);
	uint32_t swap_slot = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
	lock_release (&swap_lock);
	ASSERT (swap_slot != BITMAP_ERROR);
	return swap_slot;
}

void swap_read_page (uint32_t swap_slot, void *upage)
{
	ASSERT (bitmap_all (swap_bitmap, swap_slot, 1));
	struct block *swap_block = block_get_role (BLOCK_SWAP);
	int sector_count;
	block_sector_t block_sector;
	for (sector_count = 0; sector_count < PGSIZE / BLOCK_SECTOR_SIZE; sector_count ++)
	{
		block_sector = swap_slot * PGSIZE / BLOCK_SECTOR_SIZE + sector_count;
		block_read (swap_block, block_sector, upage + sector_count * BLOCK_SECTOR_SIZE);
	}
}

void swap_write_page (uint32_t swap_slot, void *upage)
{
	ASSERT (bitmap_all (swap_bitmap, swap_slot, 1));
	struct block *swap_block = block_get_role (BLOCK_SWAP);
	int sector_count;
	block_sector_t block_sector;
	for (sector_count = 0; sector_count < PGSIZE / BLOCK_SECTOR_SIZE; sector_count ++)
	{
		block_sector = swap_slot * PGSIZE / BLOCK_SECTOR_SIZE + sector_count;
		block_write (swap_block, block_sector, upage + sector_count * BLOCK_SECTOR_SIZE);
	}
}

