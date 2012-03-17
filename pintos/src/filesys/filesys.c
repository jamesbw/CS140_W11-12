#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  cache_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_flush ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *pathname, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char name[NAME_MAX + 1];
  struct dir *dir;

  //reject trailing '/'
  if (pathname[strlen (pathname)- 1] == '/')
    return NULL;

  if (!dir_parse_pathname (pathname, &dir, name))
    return false;
  dir_lock (dir);


  bool success = (dir != NULL
                  && !inode_is_removed (dir_get_inode (dir))
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_unlock (dir);
  dir_close (dir);

  return success;
}

/* Opens the file or directory with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
void *
filesys_open (const char *pathname, bool *is_dir)
{
  char name[NAME_MAX + 1];
  struct dir *dir;

  if (!dir_parse_pathname (pathname, &dir, name))
    return NULL;
  dir_lock (dir);

  if (inode_is_removed (dir_get_inode (dir)))
  {
    dir_unlock (dir);
    dir_close (dir);
    return NULL;
  }

  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_unlock (dir);
  dir_close (dir);

  if (inode == NULL)
    return NULL;

  if (inode_is_directory (inode))
  {
    if (is_dir != NULL)
      *is_dir = true;
    return (void *)dir_open (inode);
  }
  else
  {
    if (pathname[strlen (pathname)- 1] == '/')
      return NULL;
    if (is_dir != NULL)
      *is_dir = false;
    return (void *)file_open (inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *pathname) 
{
  char name[NAME_MAX + 1];
  struct dir *dir;

  struct inode *inode;

  // if (!strcmp (name, ".") || !strcmp (name, ".."))
  //   return false;
  //TODO

  if (!dir_parse_pathname (pathname, &dir, name))
    return false;
  dir_lock (dir);

  if ((!strcmp (name, "..")) || !dir_lookup (dir, name, &inode))
  {
    dir_unlock (dir);
    dir_close (dir);
    return false;
  }

  if (inode_is_directory (inode))
  {
    struct dir *dir_to_remove = dir_open (inode);
    if (strcmp (name, "."))
    {
      dir_lock (dir_to_remove);
    }
    if (dir_get_num_entries (dir_to_remove) > 2)
    {
      if (strcmp (name, "."))
      {
        dir_unlock (dir_to_remove);
      }
      dir_unlock (dir);
      dir_close (dir_to_remove);
      dir_close (dir);
      inode_close (inode);
      return false;
    }
    if (strcmp (name, "."))
    {
      dir_unlock (dir_to_remove);
    }
    dir_unlock (dir_to_remove);
    dir_close (dir_to_remove);
  }
  else
  if (pathname[strlen (pathname) -1] == '/')
    //invalid file name
  {
    dir_unlock (dir);
    dir_close (dir);
    inode_close (inode);
    return false;
  }

  bool success = dir_remove (dir, name);
  dir_unlock (dir);
  dir_close (dir);
  inode_close (inode);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
