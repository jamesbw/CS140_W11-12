#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <console.h>
#include "pagedir.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "process.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "filesys/directory.h"

#include "vm/page.h"
#include "vm/frame.h"
#include <hash.h>
#include <string.h>
#include "filesys/inode.h"

static void syscall_handler (struct intr_frame *);
static void verify_uaddr ( void *uaddr);
static void check_buffer_uaddr ( void *buf, int size);
static void pin_buffer (void *buf, int size);
static void unpin_buffer (void *buf, int size);

// Prototypes for each system call called by the handler.
void syscall_halt (void);
void syscall_exit (struct intr_frame *f, uint32_t status);
void syscall_exec (struct intr_frame *f, uint32_t file_name);
void syscall_wait (struct intr_frame *f, uint32_t tid);
void syscall_create (struct intr_frame *f, uint32_t file_name, uint32_t i_size);
void syscall_remove (struct intr_frame *f, uint32_t file_name);
void syscall_open (struct intr_frame *f, uint32_t file_name);
void syscall_filesize (struct intr_frame *f, uint32_t fd);
void syscall_read (struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_write(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_seek (struct intr_frame *f, uint32_t fd, uint32_t position);
void syscall_tell (struct intr_frame *f, uint32_t fd);
void syscall_close (struct intr_frame *f, uint32_t fd);
void syscall_mmap (struct intr_frame *f, uint32_t fd, uint32_t vaddr);
void syscall_munmap (struct intr_frame *f, uint32_t mapid);
void syscall_chdir (struct intr_frame *f, uint32_t dir_name);
void syscall_mkdir (struct intr_frame *f, uint32_t dir_name);
void syscall_readdir (struct intr_frame *f, uint32_t fd, uint32_t name);
void syscall_isdir (struct intr_frame *f, uint32_t fd);
void syscall_inumber (struct intr_frame *f, uint32_t fd);



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t * k_esp = (uint32_t *) (f->esp);
  verify_uaddr (k_esp);
  uint32_t syscall_number = (uint32_t) *(k_esp); 

  thread_current ()->esp = k_esp;

  uint32_t arg1 = 0; //Initialized to prevent compiler warnings
  uint32_t arg2 = 0;
  uint32_t arg3 = 0;


  // Initializing only the arguments that are needed
  switch (syscall_number)
  {
    case SYS_READ:
    case SYS_WRITE:
      verify_uaddr (f->esp + 12);
      arg3 = *(uint32_t *) (f->esp + 12);
    case SYS_CREATE:
    case SYS_SEEK:
    case SYS_MMAP:
    case SYS_READDIR:
      verify_uaddr (f->esp + 8);
      arg2 = *(uint32_t *) (f->esp + 8);
    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
    case SYS_MUNMAP:
    case SYS_CHDIR:
    case SYS_MKDIR:
    case SYS_ISDIR:
    case SYS_INUMBER:
      verify_uaddr (f->esp + 4);
      arg1 = *(uint32_t *) (f->esp + 4);
    case SYS_HALT:
      break;
  }

  switch (syscall_number)
  {
      case SYS_HALT:
      	syscall_halt ();
        break;
      case SYS_EXIT:
      	syscall_exit (f, arg1);
      	break;
      case SYS_EXEC:
      	syscall_exec (f, arg1);
        break;
      case SYS_WAIT:
      	syscall_wait (f, arg1);
        break;
      case SYS_CREATE:
      	syscall_create (f, arg1, arg2);
        break;
      case SYS_REMOVE:
      	syscall_remove (f, arg1);
        break;
      case SYS_OPEN:
      	syscall_open (f, arg1);
      	break;
      case SYS_FILESIZE:
      	syscall_filesize (f, arg1);
      	break;
      case SYS_READ:
      	syscall_read (f, arg1, arg2, arg3);
      	break;
      case SYS_WRITE:
      	syscall_write (f, arg1, arg2, arg3);
        break;        
      case SYS_SEEK:
      	syscall_seek (f, arg1, arg2);
      	break;
      case SYS_TELL:
      	syscall_tell (f, arg1);
      	break;
      case SYS_CLOSE:
      	syscall_close (f, arg1);
      	break;
      case SYS_MMAP:
        syscall_mmap (f, arg1, arg2);
        break;
      case SYS_MUNMAP:
        syscall_munmap (f, arg1);
        break;
      case SYS_CHDIR:
        syscall_chdir (f, arg1);
        break;
      case SYS_MKDIR:
        syscall_mkdir (f, arg1);
        break;
      case SYS_READDIR:
        syscall_readdir (f, arg1, arg2);
        break;
      case SYS_ISDIR:
        syscall_isdir (f, arg1);
        break;
      case SYS_INUMBER:
        syscall_inumber (f, arg1);
        break;
      default:
        printf ("system call!\n");
        thread_exit ();
      	break;
  }
}

void syscall_halt (void) 
{
  shutdown_power_off ();
}

void syscall_exit (struct intr_frame *f, uint32_t status) 
{
  f->eax = status;
  //Update exit code
  struct list_elem *e;
  struct process *p;
  lock_acquire (&process_lock);
  for (e = list_begin(&process_list); e != list_end(&process_list); e =
	 list_next (e)) {
    p = list_entry(e, struct process, elem);
    if (p->tid == thread_current()->tid) {
      p->exit_code = status;
      break;
    }
  }
  lock_release (&process_lock);
  thread_exit ();
}

void syscall_exec (struct intr_frame *f, uint32_t file_name) 
{
  verify_uaddr ((char *) file_name);
  f->eax = process_execute ((char *) file_name);
}

void syscall_wait (struct intr_frame *f, uint32_t tid) 
{
  f->eax = process_wait ( (tid_t) tid);
}

void syscall_create (struct intr_frame *f, uint32_t file_name, uint32_t i_size)
{
  verify_uaddr ((char *) file_name);
  f->eax = filesys_create ( (char *) file_name, i_size);
}

void syscall_remove (struct intr_frame *f, uint32_t file_name) 
{
  verify_uaddr ((char *) file_name);
  f->eax = filesys_remove ( (char *) file_name);
}

void syscall_open (struct intr_frame *f, uint32_t file_name) 
{
  bool is_dir;

  verify_uaddr ((char *) file_name);
  pin_buffer ((char *) file_name, strlen ((char *) file_name));
  lock_acquire (&filesys_lock);
  void *file_or_dir = filesys_open ( (char *) file_name, &is_dir);
  if (file_or_dir == NULL) {
    f->eax = -1;
  } else {
    struct file_wrapper *fw = wrap_file (file_or_dir, is_dir); 
    list_push_back (&thread_current ()->open_files, &fw->elem);   
    f->eax = fw->fd;
  }
  lock_release (&filesys_lock);
  unpin_buffer ((char *) file_name, strlen ((char *) file_name));
}

void syscall_filesize (struct intr_frame *f, uint32_t fd) 
{
  if (fd == 0 || fd ==1)
    f->eax = 0;
  else
  {    
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL || fw->is_dir)
      f->eax = -1;
    else
    {    
      lock_acquire (&filesys_lock);
      f->eax = file_length ((struct file *)fw->file_or_dir);
      lock_release (&filesys_lock);
    }
  }
}

void syscall_read (struct intr_frame *f, uint32_t fd, uint32_t buffer,
		  uint32_t length)
{
  char *buf = (void *) buffer;
  int size = length;
  check_buffer_uaddr (buf, size);
  
  if (fd  == 0) { //Read from Keyboard
    int count = 0;
    while (size - count > 0) {
      buf[count] = input_getc ();
      count ++;
    }
  } else {
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL || fw->is_dir) {
      f->eax =  -1;
    } else {
      pin_buffer (buf, size);
      lock_acquire (&filesys_lock);
      f->eax = file_read ((struct file *)fw->file_or_dir, buf, size);
      lock_release (&filesys_lock);
      unpin_buffer (buf, size);
    }
  }
}

void syscall_write (struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length) 
{
  char *buf = (void *) buffer;
  int size = length;
  check_buffer_uaddr (buf, size);
  int BUF_CHUNK = 200;
  if (fd == 1) {  //Write to console 
    while (size > BUF_CHUNK) {
      putbuf(buf, BUF_CHUNK);
      size -= BUF_CHUNK;
    }
    putbuf(buf, size);
    size = length;
    f->eax = size;
  } else {
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL || fw->is_dir) {
      f->eax =  -1;
    } else {
      pin_buffer (buf, size);
      lock_acquire (&filesys_lock);
      f->eax = file_write ((struct file *)fw->file_or_dir, buf, size);
      lock_release (&filesys_lock);
      unpin_buffer (buf, size);
    }
  }
}

void syscall_seek (struct intr_frame *f UNUSED, uint32_t fd, uint32_t position) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL && !fw->is_dir) { 
    lock_acquire (&filesys_lock);
    file_seek ((struct file *)fw->file_or_dir, position);
    lock_release (&filesys_lock);
  }
}

void syscall_tell (struct intr_frame *f, uint32_t fd) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw == NULL || fw->is_dir) {
    f->eax =  -1;
  } else {
    lock_acquire (&filesys_lock);
    f->eax = file_tell ((struct file *)fw->file_or_dir);
    lock_release (&filesys_lock);
  }
}

void syscall_close (struct intr_frame *f UNUSED, uint32_t fd) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) {
    list_remove (&fw->elem);
    if (fw->is_dir)
    {
      lock_acquire (&filesys_lock);
      dir_close ((struct dir *)fw->file_or_dir);
      lock_release (&filesys_lock);
    }
    else
    {
      lock_acquire (&filesys_lock);
      file_close ((struct file *)fw->file_or_dir);
      lock_release (&filesys_lock);
    }
    free (fw);
  }
}

void 
syscall_mmap (struct intr_frame *f, uint32_t fd, uint32_t vaddr_)
{
  void *vaddr = (void *)vaddr_;
  if (!is_user_vaddr (vaddr))
    thread_exit (); // Not user address

  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if ((fw == NULL || fw->is_dir)
    || (file_length ((struct file *)fw->file_or_dir) == 0)
    || ((uint32_t) vaddr % PGSIZE != 0)
    || (vaddr == 0)) {
    f->eax =  MAP_FAILED;
    return;
  }

  //check that no pages are mapped in the space needed for the file
  int num_pages = (file_length ((struct file *)fw->file_or_dir) -1 )/ PGSIZE + 1;
  off_t offset;
  for (offset = 0; offset < num_pages *PGSIZE; offset += PGSIZE)
  {
    if (page_lookup (thread_current ()->supp_page_table, vaddr + offset))
    {
      f->eax =  MAP_FAILED;
      return;
    }
  }

  mapid_t mapid = fd; 

  struct mmapped_file *mf = malloc (sizeof (struct mmapped_file));
  mf->file = file_reopen ((struct file *)fw->file_or_dir);
  ASSERT (mf->file);
  mf->base_page = vaddr;
  mf->mapid = mapid;

  int size_left = file_length ((struct file *)fw->file_or_dir);
  for (offset = 0; offset < num_pages *PGSIZE; offset += PGSIZE)
  {
    int valid_bytes = size_left < PGSIZE ? size_left : PGSIZE;
    page_insert_mmapped (vaddr + offset, mapid, mf->file, offset, valid_bytes);
  }

  list_push_back (&thread_current ()->mmapped_files, &mf->elem);

  f->eax = mapid;

}
void 
syscall_munmap (struct intr_frame *f UNUSED, uint32_t mapid)
{

  process_munmap ( (mapid_t) mapid);

}


void 
syscall_chdir (struct intr_frame *f UNUSED, uint32_t dir_name UNUSED)
{

}

void 
syscall_mkdir (struct intr_frame *f UNUSED, uint32_t dir_name UNUSED)
{

}

void 
syscall_readdir (struct intr_frame *f UNUSED, uint32_t fd, uint32_t name_)
{
  char *name = (char *) name_;
  verify_uaddr ( name);

  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL || !fw->is_dir) 
    return;
  else
    dir_readdir ((struct dir *)fw->file_or_dir, name);

}

void 
syscall_isdir (struct intr_frame *f, uint32_t fd)
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) 
    f->eax = fw->is_dir;
  else
    f->eax = false; 
}

void 
syscall_inumber (struct intr_frame *f, uint32_t fd)
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) 
  {
    if (fw->is_dir)
    {
      struct dir *dir = (struct dir *) fw->file_or_dir;
      lock_acquire (&filesys_lock);
      f->eax = inode_get_inumber (dir_get_inode (dir));
      lock_release (&filesys_lock);
    }
    else
    {
      struct file *file = (struct file *) fw->file_or_dir;
      lock_acquire (&filesys_lock);
      f->eax = inode_get_inumber (file_get_inode (file));
      lock_release (&filesys_lock);
    }
    
  }
  else
    f->eax = -1;
}


static void
verify_uaddr (void *uaddr)
{
  if (!is_user_vaddr (uaddr))
    thread_exit (); // Not user address
  if ( page_lookup (thread_current ()->supp_page_table, pg_round_down (uaddr)) == NULL)
  {
    if ( page_stack_access (uaddr, thread_current ()->esp) )
      page_extend_stack (uaddr);
    else
      thread_exit (); // Not mapped
  }
}


/*check the start and end of buffer, and one address every PGSIZE
in between*/
static void
check_buffer_uaddr (void *buf, int size)
{
  verify_uaddr (buf);
  int i;
  for(i = 1; i <= (size -2 )/ PGSIZE; i++)
    verify_uaddr (buf + i*PGSIZE);
  verify_uaddr (buf + size - 1);
}

static void 
pin_buffer (void *buf, int size)
{
  frame_pin (buf);
  int i;
  for(i = 1; i <= (size -2 )/ PGSIZE; i++)
    frame_pin (buf + i*PGSIZE);
  frame_pin (buf + size - 1);
}

static void
unpin_buffer (void *buf, int size)
{
  frame_unpin (buf);
  int i;
  for(i = 1; i <= (size -2 )/ PGSIZE; i++)
    frame_unpin (buf + i*PGSIZE);
  frame_unpin (buf + size - 1);
}
