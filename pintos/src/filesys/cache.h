#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 65 // 64 sectors + free map
#define WRITE_BEHIND_INTERVAL (1 /10) //in seconds

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

struct queued_sector
{
	block_sector_t sector;
	struct list_elem elem;
};

struct list read_ahead_queue;
struct lock read_ahead_lock;
struct condition read_ahead_go;

void read_ahead_func (void *aux);
void cache_read_ahead (block_sector_t sector);

struct cached_block block_cache[CACHE_SIZE];
struct lock cache_lock;

void cache_init (void);
struct cached_block *cache_run_clock (void);
void cache_flush (void);
struct cached_block *cache_insert (block_sector_t sector);
void write_behind_func (void *aux);


#endif
