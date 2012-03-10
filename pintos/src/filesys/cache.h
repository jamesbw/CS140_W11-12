#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64 // 64 sectors

struct cached_block
{
	uint8_t data[BLOCK_SECTOR_SIZE];
	block_sector_t sector;
	struct lock lock;
};

struct cached_block block_cache[CACHE_SIZE];

void cache_init (void);

#endif
