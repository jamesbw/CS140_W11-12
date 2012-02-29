#ifndef VM_SHARING_H
#define VM_SHARING_H

#include <hash.h>
#include <list.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "page.h"
#include "filesys/inode.h"

struct hash executable_table;
struct lock executable_table_lock;

struct shared_executable
{
	struct inode *inode;
	off_t offset;
	// void *kpage;
	struct lock busy;
	struct list user_pages;
	struct hash_elem elem;
};

bool sharing_scan_and_clear_accessed_bit (struct page *page);
unsigned exec_hash (const struct hash_elem *p_, void *aux );
bool exec_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux );
void sharing_register_page (struct page *page);
void sharing_unregister_page (struct page *page);
struct shared_executable *sharing_lookup (struct page *page);
bool sharing_scan_and_clear_accessed_bit (struct page *page);
void *sharing_find_shared_frame (struct page *page);
void sharing_invalidate (struct page *page);
bool sharing_pinned (struct page *page);


#endif
