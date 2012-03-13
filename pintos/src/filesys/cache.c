#include "cache.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "filesys.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/timer.h"

struct cached_block block_cache[CACHE_SIZE];

uint8_t cache_hand;



void 
cache_init (void)
{
	int i;
	for (i = 0; i < CACHE_SIZE; i++)
	{
		lock_init (&block_cache[i].lock);
		cond_init (&block_cache[i].r_w_done);
	}

	cache_hand = 0;
	lock_init (&cache_lock);


	thread_create ("write-behind", PRI_DEFAULT, write_behind_func, NULL);
}

void write_behind_func (void *aux UNUSED)
{
	while (true)
	{
		timer_sleep (TIMER_FREQ * 1);
		cache_flush ();
	}
}

// struct cached_block *
// cache_find_unused (block_sector_t sector)
// {
// 	int i;
// 	struct cached_block *b;
// 	lock_acquire (&cache_lock);
// 	for (i = 0; i < CACHE_SIZE; i++)
// 	{
// 		b = &block_cache[i];
// 		if (!b->in_use)
// 		{
// 			b->pinned = true;
// 			b->in_use = true;
// 			b->sector = sector;
// 			lock_release (&cache_lock);
// 			return b;
// 		}
// 	}
// 	lock_release (&cache_lock);
// 	return NULL;
// }

// struct cached_block *
// cache_allocate (block_sector_t sector)
// {
// 	struct cached_block *b = cache_find_unused (sector);
// 	if (b == NULL)
// 		b = cache_evict (sector);
// 	return b;
// }

// struct cached_block *
// cache_evict (block_sector_t new_sector)
// {
// 	lock_acquire (&cache_lock);

// 	struct cached_block *block_to_evict = cache_run_clock ();

// 	lock_acquire( &block_to_evict->lock);
// 	block_to_evict->pinned = true;
// 	block_sector_t old_sector = block_to_evict->sector;
// 	block_to_evict->sector = new_sector;

// 	lock_release (&cache_lock);

// 	if (block_to_evict->dirty)
// 		block_write (fs_device, old_sector, block_to_evict->data);
// 	block_to_evict->dirty = false;
// 	block_to_evict->accessed = false;

// 	lock_release (&block_to_evict->lock);

// 	return block_to_evict;
// }

struct cached_block *
cache_run_clock (void)
{
	ASSERT (lock_held_by_current_thread (&cache_lock));

	struct cached_block *b = NULL;
	while (true)
	{
		cache_hand = (cache_hand + 1) % CACHE_SIZE;
		b = &block_cache[cache_hand];
		if (!b->accessed)
		{
			if (!b->IO_needed)
				return b;
		}
		else
			b->accessed = false;			
	}

	//TODO: keep looping or wait on a condition variable?
}


// struct cached_block *
// cache_lookup (block_sector_t sector)
// {
// 	int i;
// 	struct cached_block *b;
// 	lock_acquire (&cache_lock);
// 	for (i = 0; i < CACHE_SIZE; i++)
// 	{
// 		b = &block_cache[i];
// 		if (b->sector == sector && b->in_use)
// 		{
// 			b->pinned = true;
// 			lock_release (&cache_lock);
// 			return b;
// 		}
// 	}
// 	lock_release (&cache_lock);
// 	return NULL;
// }





struct cached_block *
cache_insert (block_sector_t sector)
{

	int i;
	struct cached_block *b;
	struct cached_block *empty_b = NULL;
	bool already_present = false;
	// bool write_needed = false;
	// bool read_needed = false;
	// block_sector_t old_block;

	lock_acquire (&cache_lock);
	for (i = 0; i < CACHE_SIZE; i++)
	{
		b = &block_cache[i];
		if (!b->in_use)
		{
			empty_b = b;
		}
		else if (b->sector == sector )
		{
			already_present = true;		
			break;
		}
	}
	if (!already_present)
	{
		b = empty_b;
		if (b == NULL)
		{
			b = cache_run_clock ();
			// write_needed = b->dirty;
			b->old_sector = b->sector;
		}
		b->sector = sector;
		b->IO_needed = true;

		b->in_use = true;
		// read_needed = true;
	}

	// b->pinned = true;
	lock_release (&cache_lock);
	lock_acquire (&b->lock);

	if (b->IO_needed)
	{
		while (b->active_r_w > 0)
		{
			cond_wait (&b->r_w_done, &b->lock);
		}
		if (b->dirty)
		{
			block_write (fs_device, b->old_sector, b->data);
		}
		block_read (fs_device, b->sector, b->data);
		b->IO_needed = false;
		b->accessed = false;
		b->dirty = false;
	}





	// if (write_needed || read_needed)
	// {
	// 	while (b->active_r_w > 0)
	// 	{
	// 		cond_wait (&b->r_w_done, &b->lock);
	// 	}
	// 	if (write_needed)
	// 		block_write (fs_device, old_sector, b->data);
	// 	if (read_needed)
	// 		block_read (fs_device, sector, b->data);
	// 	b->IO_in_progress = false;
	// 	b->accessed = false;
	// 	b->dirty = false;
	// 	cond_broadcast (&b->cond_io_done, &b->lock);
	// }

	// lock_release (&b->lock);
	return b;	
}


void 
cache_flush (void)
{
	int i;
	struct cached_block *b;
	block_sector_t sector;
	for (i = 0; i < CACHE_SIZE; i++)
	{
		b = &block_cache[i];
		lock_acquire (&b->lock);
		while (b->active_r_w > 0)
		{
			cond_wait (&b->r_w_done, &b->lock);
		}
		if (b->in_use && b->dirty)
		{
			sector = b->sector;
			if (b->IO_needed)
				sector = b->old_sector;
			block_write (fs_device, sector, b->data);
			b->dirty = false;
		}
		lock_release (&b->lock);
	}
}











