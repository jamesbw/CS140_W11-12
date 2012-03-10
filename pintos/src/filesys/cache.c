#include "cache.h"
#include "threads/synch.h"
#include "devices/block.h"

struct cached_block block_cache[CACHE_SIZE];



void 
cache_init (void)
{
	int i;
	for (i = 0; i < CACHE_SIZE; i ++)
		lock_init (&block_cache[i]->lock);
}