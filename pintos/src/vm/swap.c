#include "swap.h"
#include "devices/block.h"
#include <kernel/bitmap.h>
#include "threads/vaddr.h"


void swap_init (void)
{
	struct block *swap_block = block_get_role (BLOCK_SWAP);
	swap_bitmap = bitmap_create (block_size (swap_block) * BLOCK_SECTOR_SIZE / PGSIZE);
}

void swap_free (uint32_t swap_slot)
{
	bitmap_reset (swap_bitmap, swap_slot);
}

uint32_t swap_allocate_slot (void)
{
	return bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
}

void swap_read_page (uint32_t swap_slot, void *upage)
{
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
	struct block *swap_block = block_get_role (BLOCK_SWAP);
	int sector_count;
	block_sector_t block_sector;
	for (sector_count = 0; sector_count < PGSIZE / BLOCK_SECTOR_SIZE; sector_count ++)
	{
		block_sector = swap_slot * PGSIZE / BLOCK_SECTOR_SIZE + sector_count;
		block_write (swap_block, block_sector, upage + sector_count * BLOCK_SECTOR_SIZE);
	}
}

