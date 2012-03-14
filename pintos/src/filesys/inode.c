#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "cache.h"
#include "threads/thread.h"
#include <stdio.h>


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT_BLOCKS 12
#define BLOCKS_PER_INDIRECT (BLOCK_SECTOR_SIZE / sizeof block_sector_t)

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk
//   {
//     block_sector_t start;               /* First data sector. */
//     off_t length;                       /* File size in bytes. */
//     unsigned magic;                     /* Magic number. */
//     uint32_t unused[125];               /* Not used. */
//   };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    off_t length;
    block_sector_t direct_blocks[NUM_DIRECT_BLOCKS];
    block_sector_t indirect_block;
    block_sector_t doubly_indirect_block;
    unsigned magic;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  block_sector_t block_buf[BLOCKS_PER_INDIRECT];
  if (pos < inode->length)
  {
    int block_num = pos / BLOCK_SECTOR_SIZE;
    if (block_num < NUM_DIRECT_BLOCKS)
    {
      return inode->direct_blocks[block_num];
    }
    if (block_num < NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT)
    {
      block_read (fs_device, inode->indirect_block, block_buf);
      return block_buf[block_num - NUM_DIRECT_BLOCKS];
    }
    else
    {
      int indirect_block_num = ( block_num - (NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT )) / BLOCKS_PER_INDIRECT;
      block_read (fs_device, inode->indirect_block, block_buf);
      block_read (fs_device, block_buf[indirect_block_num], block_buf);
      int final_block_index = ( block_num - (NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT )) % BLOCKS_PER_INDIRECT;
      return block_buf[final_block_index];

    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */

//TODO lock this list
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector_, off_t length)
{
  uint8_t buf[BLOCK_SECTOR_SIZE];
  block_sector_t *ind_block_buf = (block_sector_t *) buf;
  block_sector_t db_ind_block_buf[BLOCKS_PER_INDIRECT];
  static char zeros[BLOCK_SECTOR_SIZE];
  bool success = false;

  int indirect_block_num ;
  int final_block_index ;


  ASSERT (length >= 0);
  struct inode *inode = malloc (sizeof (struct inode));
  if (inode != NULL)
  {


    inode->sector = sector_;
    inode->length = length;
    inode->magic = INODE_MAGIC;

    size_t sectors = bytes_to_sectors (length);
    size_t block_num;
    block_sector_t sector;
    for (block_num = 0; block_num < sectors; block_num ++)
    {
      if (free_map_allocate (1, &sector))
      {
          if (block_num < NUM_DIRECT_BLOCKS)
          {
            inode->direct_blocks[block_num] = sector;
            block_write (fs_device, sector, zeros);
            continue;
          }
          if (block_num == NUM_DIRECT_BLOCKS && inode->indirect_block == 0)
            //allocate indirect block
          {
            inode->indirect_block = sector;
            block_write (fs_device, sector, zeros);
            block_num --;
            continue;

            // if (free_map_allocate (1, &sector))
            // {
            // }
            // else
            // {
            //   success = false;
            //   break;
            // }
          }
          if (block_num < NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT)
          {
            // if (free_map_allocate (1, &sector))
            // {
            ind_block_buf[block_num - NUM_DIRECT_BLOCKS] = sector;
            block_write (fs_device, sector, zeros);
          
            // else
            // {
            //   success = false;
            //   break;
            // }
            if ((block_num == sectors - 1) || (block_num == NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT - 1)) 
            //last block or end of indirect block
            {
              block_write (fs_device, inode->indirect_block, ind_block_buf);
              memset (ind_block_buf, 0, BLOCK_SECTOR_SIZE);
            }
            continue;
          }
          
          if ((block_num == NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT)
              && inode->doubly_indirect_block == 0)
            // allocate doubly indirect block
          {
              inode->doubly_indirect_block = sector;
              block_write (fs_device, sector, zeros);
              block_num -- ;
              continue;

            // if (free_map_allocate (1, &sector))
            // {
            // }
            // else
            // {
            //   success = false;
            //   break;
            // }
          }
          if (block_num >= NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT)
          {
            indirect_block_num = ( block_num - (NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT )) / BLOCKS_PER_INDIRECT;
            final_block_index = ( block_num - (NUM_DIRECT_BLOCKS + BLOCKS_PER_INDIRECT )) % BLOCKS_PER_INDIRECT;

            if ((final_block_index == 0) && 
                (db_ind_block_buf[indirect_block_num] == 0))// new indirect block
            {
              db_ind_block_buf[indirect_block_num] = sector;
              block_write (fs_device, sector, zeros);
              block_num--;
              continue;
              // if (free_map_allocate (1, &sector))
              // {
              // }
              // else
              // {
              //   success = false;
              //   break;
              // }
            }
            // if (free_map_allocate (1, &sector))
            // {
              ind_block_buf[final_block_index] = sector;
              block_write (fs_device, sector, zeros);
            // }
            // else
            // {
            //   success = false;
            //   break;
            // }
            if ((block_num == sectors - 1) || (final_block_index == BLOCKS_PER_INDIRECT - 1)) 
            //last block or end of indirect block
            {
              block_write (fs_device, db_ind_block_buf[indirect_block_num], ind_block_buf);
              memset (ind_block_buf, 0, BLOCK_SECTOR_SIZE);
            }
            if ((block_num == sectors - 1) ) 
            //last block 
            {
              block_write (fs_device, inode->doubly_indirect_block, db_ind_block_buf);
              memset (db_ind_block_buf, 0, BLOCK_SECTOR_SIZE);
            } 
          }
      }
      else
      {
        success = false;


        //write all outstanding buffers to disk.
        // This makes rolling back easier
        if (inode->indirect_block !=0)
        {
          if (inode->doubly_indirect_block !=0)
          {
            block_write (fs_device, db_ind_block_buf[indirect_block_num], ind_block_buf);
            block_write (fs_device, inode->doubly_indirect_block, db_ind_block_buf);
          }
          else
          {
            block_write (fs_device, inode->indirect_block, ind_block_buf);
          }
        }
        break;
      }
    }

    if (success == false) //allocation fails along the way, roll back allocations
    {
      inode_release_allocated_sectors (inode);
    }
    else
    {
      memset (buf, 0, BLOCK_SECTOR_SIZE);
      memcpy (buf, inode, sizeof (*inode));
      block_write (fs_device, inode->sector, buf);
    }
    free (inode);
  }
  return success;



  // disk_inode = calloc (1, sizeof *disk_inode);
  // if (disk_inode != NULL)
  //   {
  //     size_t sectors = bytes_to_sectors (length);
  //     disk_inode->length = length;
  //     disk_inode->magic = INODE_MAGIC;
  //     if (free_map_allocate (sectors, &disk_inode->start)) 
  //       {
  //         block_write (fs_device, sector, disk_inode);
  //         if (sectors > 0) 
  //           {
  //             static char zeros[BLOCK_SECTOR_SIZE];
  //             size_t i;
              
  //             for (i = 0; i < sectors; i++) 
  //               block_write (fs_device, disk_inode->start + i, zeros);
  //           }
  //         success = true; 
  //       } 
  //     free (disk_inode);
  //   }
  // return success;
}

void inode_release_allocated_sectors (struct inode *inode)
{
  int direct_block_num;
  int indirect_block_num;
  int final_block_index;
  block_sector_t ind_block_buf[BLOCKS_PER_INDIRECT];
  block_sector_t db_ind_block_buf[BLOCKS_PER_INDIRECT];
  block_sector_t sector;

  //deallocate direct blocks
  for (direct_block_num = 0; direct_block_num < NUM_DIRECT_BLOCKS; direct_block_num++)
  {
    sector = inode->direct_blocks[direct_block_num];
    if (sector != 0)
    {
      free_map_release (sector, 1);
    }
    else
    {
      return;
    }
  }

  //deallocate indirect block
  if (inode->indirect_block != 0)
  {
    block_read (fs_device, inode->indirect_block, ind_block_buf);
    free_map_release (inode->indirect_block, 1);
    for (final_block_index = 0; final_block_index < BLOCKS_PER_INDIRECT; final_block_index ++)
    {
      sector = ind_block_buf[final_block_index];
      if (sector != 0)
      {
        free_map_release (sector, 1);
      }
      else
      {
        return;
      }
    }
  }

  //deallocate doubly indirect block
  if (inode->doubly_indirect_block != 0)
  {
    block_read (fs_device, inode->doubly_indirect_block, db_ind_block_buf);
    free_map_release (inode->doubly_indirect_block, 1);

    for (indirect_block_num = 0; indirect_block_num < BLOCKS_PER_INDIRECT; indirect_block_num ++)
    {
      block_sector_t indirect_block_sector = db_ind_block_buf[indirect_block_num];
      if (indirect_block_sector != 0)
      {
        block_read (fs_device, indirect_block_sector, ind_block_buf);
        free_map_release (indirect_block_sector, 1);

        for (final_block_index = 0; final_block_index < BLOCKS_PER_INDIRECT; final_block_index ++)
        {
          sector = ind_block_buf[final_block_index];
          if (sector != 0)
          {
            free_map_release (sector, 1);
          }
          else
          {
            return ;
          }
        }
      }
      else
      {
        return;
      }
    }
  }
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{

  //TODO synchronization of list?
  struct list_elem *e;
  struct inode *inode;
  uint8_t buf[BLOCK_SECTOR_SIZE];

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  block_read (fs_device, sector, buf);
  memcpy (inode, buf, sizeof (*inode));

  ASSERT (inode->sector == sector);
  ASSERT (inode->magic == INODE_MAGIC);

  list_push_front (&open_inodes, &inode->elem);

  // inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length));
          inode_release_allocated_sectors (inode); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  struct cached_block *cached_block;
  block_sector_t next_sector = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;


      cached_block = cache_insert (sector_idx);


      cached_block->active_r_w ++ ;
      lock_release (&cached_block->lock);



      thread_current ()->cache_block_being_accessed = cached_block;
      memcpy (buffer + bytes_read, cached_block->data + sector_ofs, chunk_size);
      cached_block->accessed = true;

      thread_current ()->cache_block_being_accessed = NULL;

      lock_acquire (&cached_block->lock);
      cached_block->active_r_w --;
      if (cached_block->active_r_w == 0)
        cond_broadcast (&cached_block->r_w_done, &cached_block->lock);
      lock_release (&cached_block->lock);


      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;

      next_sector = sector_idx + 1;
    }
    cache_read_ahead (next_sector);


  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
    //TODO edit comment
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  struct cached_block *cached_block;
  block_sector_t next_sector = 0;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cached_block = cache_insert (sector_idx);

      cached_block->active_r_w ++ ;
      lock_release (&cached_block->lock);


      thread_current ()->cache_block_being_accessed = cached_block;
      memcpy (cached_block->data + sector_ofs, buffer + bytes_written, chunk_size);
      cached_block->dirty = true;

      thread_current ()->cache_block_being_accessed = NULL;

      lock_acquire (&cached_block->lock);
      cached_block->active_r_w --;
      if (cached_block->active_r_w == 0)
        cond_broadcast (&cached_block->r_w_done, &cached_block->lock);
      lock_release (&cached_block->lock);


      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;


      next_sector = sector_idx + 1;
    }
    cache_read_ahead (next_sector);


  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
