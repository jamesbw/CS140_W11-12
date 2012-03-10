#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64 // 64 sectors

struct cached_block
{
	uint8_t data[BLOCK_SECTOR_SIZE];
	block_sector_t sector;
	bool in_use;
	bool pinned;
	bool accessed;
	bool dirty;
	struct lock lock;
};

struct cached_block block_cache[CACHE_SIZE];
struct lock cache_lock;

void cache_init (void);
struct cached_block *cache_lookup (block_sector_t sector);

#endif
