#include "sharing.h"
#include <hash.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "frame.h"
#include "page.h"


unsigned 
exec_hash (const struct hash_elem *se_, void *aux UNUSED) {
    const struct shared_executable *se = hash_entry(se_, struct shared_executable, elem);
    char buf[sizeof (off_t) + sizeof (block_sector_t)];
    memset (buf, &se->offset, sizeof (off_t));
    memset (buf + sizeof (off_t), &se->sector, sizeof (block_sector_t));
    return hash_bytes(buf, sizeof(buf));
}

bool 
exec_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct shared_executable *a = hash_entry(a_, struct shared_executable, elem);
    const struct shared_executable *b = hash_entry(b_, struct shared_executable, elem);
    return a->block < b->block || a->offset < b->offset;
}


void 
sharing_register_page (struct page *page)
{

	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	if (shared_exec)
	{
		// lock_acquire (&shared_exec->busy);
		list_push_front (&shared_exec->user_pages, page->exec_elem);
		// lock_release (&shared_exec->busy);
	}
	else
	{
		shared_exec = malloc (sizeof (struct shared_executable));
		ASSERT (shared_exec);
		shared_exec->sector = page->file->inode->sector;
		shared_exec->offset = page->offset;
		// shared_exec->kpage = NULL;
		list_init (&shared_exec->user_pages);
		// lock_init (&shared_exec->busy);

		list_push_front (&shared_exec->user_pages, page->exec_elem);

		hash_insert (&executable_table, shared_exec->elem);
	}
	lock_release (&executable_table_lock);
}

void sharing_unregister_page (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	ASSERT (shared_exec);

	// lock_acquire (&shared_exec->busy);
	list_remove (&shared_exec->user_pages, page->exec_elem);
	// lock_release (&shared_exec->busy);
	if (list_empty (&shared_exec->user_pages))
	{
		hash_delete (&executable_table, &shared_exec->elem);
		free (shared_exec);
	}
	else
	{
		lock_acquire (&frame_table_lock);
		if (hash_find (&frame_table, &page->frame_elem) == &page->frame_elem)
		{
			ASSERT (page->paddr);
			list_remove (&page->exec_elem);
			struct list_elem *e = list_begin (&shared_exec->user_pages);
			struct page *sharing_page = list_entry (e, struct page, exec_elem);
			sharing_page->paddr = page->paddr;
			pagedir_set_page (sharing_page->pd, sharing_page->vaddr, sharing_page->paddr, false);
			hash_delete (&frame_table, &page->frame_elem);
			hash_insert (&frame_table, &sharing_page->frame_elem);
		}
		page->paddr = NULL;
		pagedir_clear_page (page->pd, page->vaddr);
		lock_release (&frame_table_lock);
	}

	lock_release (&executable_table_lock);
}

struct shared_executable *
sharing_lookup (struct page *page)
{
	ASSERT (page->type == EXECUTABLE && page->writable == false);
	ASSERT (page->file);
	ASSERT (page->offset != (off_t) -1);

	struct  shared_executable se;
	se.sector = page->file->inode->sector;
	se.offset = page->offset;

	struct hash_elem *e = hash_find (&executable_table, se.elem);

	if (e)
		return hash_entry (e, struct shared_executable, elem);
	else
		return NULL;
}

bool 
sharing_scan_and_clear_accessed_bit (struct page *page)
{
	bool accessed = false;

	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      accessed = pagedir_is_accessed (sharing_page->pd, sharing_page->vaddr);
      pagedir_set_accessed (sharing_page->pd, sharing_page->vaddr, false);
    }

    return accessed;
}

void *
sharing_find_shared_frame (struct page *page)
{
	lock_acquire (&executable_table_lock);
    struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      if (sharing_page->paddr)
      {
      	lock_release (&executable_table_lock);
      	return paddr;
      }
    }
    lock_release (&executable_table_lock);
    return NULL;
}

void
sharing_invalidate (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      sharing_page->paddr = 0;
      pagedir_clear_page (sharing_page->pd, sharing_page->vaddr);
    }
	lock_release (&executable_table_lock);
}

bool
sharing_pinned (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      if (sharing_page->pinned)
      {
      	lock_release (&executable_table_lock);
      	return true;
      }
    }
	lock_release (&executable_table_lock);
	return false;
}

