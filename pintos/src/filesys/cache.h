#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64 // 64 sectors

struct cached_block
{
	uint8_t data[BLOCK_SECTOR_SIZE];
	block_sector_t sector;
	block_sector_t old_sector;
	bool in_use;
	int active_r_w;
	bool accessed;
	bool dirty;
	bool IO_needed;
	struct lock lock;
	struct condition r_w_done;
};

struct cached_block block_cache[CACHE_SIZE];
struct lock cache_lock;

void cache_init (void);
// struct cached_block *cache_lookup (block_sector_t sector);
// struct cached_block *cache_find_unused (block_sector_t sector);
// struct cached_block *cache_allocate (block_sector_t sector);
// struct cached_block *cache_evict (block_sector_t new_sector);
struct cached_block *cache_run_clock (void);
void cache_flush (void);
struct cached_block *cache_insert (block_sector_t sector);


#endif
