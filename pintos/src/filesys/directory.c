#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "free-map.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, block_sector_t parent_sector, size_t entry_cnt)
{
  entry_cnt += 2; //to account for "." and ".."
  if (!inode_create (sector, entry_cnt * sizeof (struct dir_entry), true))
    return false;

  struct inode *inode = inode_open (sector);

  struct dir *dir = dir_open (inode);
  dir_add (dir, ".", sector);
  dir_add (dir, "..", parent_sector);
  dir_close (dir);
  return true;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL && inode_is_directory (inode))
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if ((e.in_use)
          && strcmp (e.name, ".")
          && strcmp (e.name, ".."))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

bool 
dir_parse_pathname (const char *pathname, struct dir **parent_dir, char *name)
{

  if ((pathname == NULL) || (pathname[0] == '\0'))
    return false;

  char *path_copy = malloc (strlen (pathname) +1);
  if (path_copy == NULL)
    return false;

  strlcpy (path_copy, pathname, strlen (pathname) +1);

  // block_sector_t starting_block;
  // struct dir *dir;
  // struct inode *inode;

  struct inode *current_inode;
  struct inode *next_inode = NULL;

  if (path_copy[0] == '/')
  {
    // *parent_dir = dir_open_root;
    // inode = inode_open (ROOT_DIR_SECTOR);
    current_inode = inode_open (ROOT_DIR_SECTOR);

    // in case the pathname refers to the root directory, give these default values
    // they get overwritten if ever there is something in the 
    *parent_dir = dir_open_root ();
    strlcpy (name, ".", strlen (".") + 1);
  }
  else
  {
    // starting_block = thread_current ()->current_dir;
    // *parent_dir = dir_open (inode_open (thread_current ()->current_dir));
    // inode = inode_open (thread_current ()->current_dir);
    // *parent_dir = dir_open (inode);
    current_inode = inode_open (thread_current ()->current_dir);
    *parent_dir = dir_open (current_inode);
  }

  char *token, *save_ptr;
  for (token = strtok_r (path_copy, "/", &save_ptr); token != NULL;
       token = strtok_r (NULL, "/", &save_ptr))
  {

    if (strlen (token) > NAME_MAX)
    {
      inode_close (next_inode);
      dir_close (*parent_dir);
      *parent_dir = NULL;
      free (path_copy);
      return false;
    }

    strlcpy (name, token, strlen (token) + 1);

    if ( next_inode != NULL )
    {
      if (inode_is_directory (next_inode))
      {
        dir_close (*parent_dir);
        current_inode = next_inode;
        *parent_dir = dir_open (current_inode);
      }
      else
      {
        inode_close (next_inode);
        dir_close (*parent_dir);
        free (path_copy);
        *parent_dir = NULL;
        return false;
      }
    }
    


    if (!dir_lookup (*parent_dir, token, &next_inode))
      break;


    // strlcpy (name, token, strlen (token) + 1);

    // if (inode_is_directory (inode))
    //   (*parent_dir)->inode = inode;
    // else
    // {
    //   free (path_copy);
    //   inode_close (inode);
    //   dir_close (*parent_dir);
    //   return false;
    // }


    // if (!dir_lookup (*parent_dir, token, &inode))
    //   break;

  }

  inode_close (next_inode);

  

  //is there still another token?
  if (strtok_r (NULL, "/", &save_ptr) != NULL)
  {
    dir_close (*parent_dir);
    free (path_copy);
    *parent_dir = NULL;
    return false;
  }

  free (path_copy);
  return true;
}


bool 
dir_create_pathname (char *pathname)
{
  char name[NAME_MAX + 1];
  struct dir *dir;

  if (!dir_parse_pathname (pathname, &dir, name))
    return false;

  block_sector_t parent_sector = inode_get_inumber (dir_get_inode (dir));

  block_sector_t inode_sector = 0;

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, parent_sector, 0)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;

}

bool 
dir_set_current_dir (char *pathname)
{
  char name[NAME_MAX + 1];
  struct dir *dir;

  if (!dir_parse_pathname (pathname, &dir, name))
    return false;

  struct inode *inode = NULL;

  bool success = ( dir_lookup (dir, name, &inode)
             && inode_is_directory (inode));

  if (success)
    thread_current ()->current_dir = inode_get_inumber (inode);
  if (inode != NULL)
    inode_close (inode);

  dir_close (dir);

  return success;          
}

size_t 
dir_get_num_entries (struct dir *dir)
{
  int count = 0;
  off_t saved_pos = dir->pos;

  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
  {
    dir->pos += sizeof e;
    if (e.in_use)
        count ++;
  }

  dir->pos = saved_pos;

  return count;
}
