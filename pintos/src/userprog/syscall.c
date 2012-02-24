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

#include "vm/page.h"
#include "vm/frame.h"
#include <hash.h>

static void syscall_handler (struct intr_frame *);
static void verify_uaddr (const void *uaddr);
static void check_buffer_uaddr (const void *buf, int size);

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
    case SYS_MUNMAP:
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
    case SYS_MMAP:
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
  verify_uaddr ((char *) file_name);
  lock_acquire (&filesys_lock);
  struct file *file = filesys_open ( (char *) file_name);
  if (file == NULL) {
    f->eax = -1;
  } else {
    struct file_wrapper *fw = wrap_file (file); 
    list_push_back (&thread_current ()->open_files, &fw->elem);   
    f->eax = fw->fd;
  }
  lock_release (&filesys_lock);
}

void syscall_filesize (struct intr_frame *f, uint32_t fd) 
{
  if (fd == 0 || fd ==1)
    f->eax = 0;
  else
  {    
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL)
      f->eax = -1;
    else
    {    
      lock_acquire (&filesys_lock);
      f->eax = file_length (fw->file);
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
    if (fw == NULL) {
      f->eax =  -1;
    } else {
      lock_acquire (&filesys_lock);
      f->eax = file_read (fw->file, buf, size);
      lock_release (&filesys_lock);
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
    if (fw == NULL) {
      f->eax =  -1;
    } else {
      lock_acquire (&filesys_lock);
      f->eax = file_write (fw->file, buf, size);
      lock_release (&filesys_lock);
    }
  }
}

void syscall_seek (struct intr_frame *f UNUSED, uint32_t fd, uint32_t position) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) { 
    lock_acquire (&filesys_lock);
    file_seek (fw->file, position);
    lock_release (&filesys_lock);
  }
}

void syscall_tell (struct intr_frame *f, uint32_t fd) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw == NULL) {
    f->eax =  -1;
  } else {
    lock_acquire (&filesys_lock);
    f->eax = file_tell (fw->file);
    lock_release (&filesys_lock);
  }
}

void syscall_close (struct intr_frame *f UNUSED, uint32_t fd) 
{
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) {
    list_remove (&fw->elem);
    lock_acquire (&filesys_lock);
    file_close (fw->file);
    lock_release (&filesys_lock);
    free (fw);
  }
}

void 
syscall_mmap (struct intr_frame *f, uint32_t fd, uint32_t vaddr_)
{
  void *vaddr = (void *)vaddr_;
  if (!is_user_vaddr (vaddr))
    thread_exit (); // Not user address

  ASSERT ((uint32_t) vaddr % PGSIZE == 0); 

  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if ((fw == NULL )
    || (file_size (fw->file) == 0)
    || (uint32_t) vaddr % PGSIZE != 0)
    || vadrr == 0) {
    f->eax =  MAP_FAILED;
    return;
  }

  //check that no pages are mapped in the space needed for the file
  int num_pages = (file_size (fw->file) -1 )/ PGSIZE + 1;
  struct page p;
  off_t offset;
  for (offset = 0; offset < num_pages *PGSIZE; offset += PGSIZE)
  {
    p.vaddr = vaddr + offset;
    if (hash_find (&page_table, &p.hash_elem))
    {
      f->eax =  MAP_FAILED;
      return;
    }
  }

  mapid_t mapid = fd; //TOOD allocate?

  int size_left = file_size (fw->file);
  for (offset = 0; offset < num_pages *PGSIZE; offset += PGSIZE)
  {
    int valid_bytes = size_left < PGSIZE ? size_left : PGSIZE;
    page_insert_mmapped (page, mf->mapid, mf->file, offset, valid_bytes);
  }

  struct mmapped_file *mf = malloc (sizeof (struct mmapped_file));
  mf->file = file_reopen (fw->file);
  ASSERT (mf->file);

  mf->base_page = vaddr;
  mf->mapid = mapid;
  list_push_back (&thread_current ()->mmapped_files, mf->elem);

  f->eax = mapid;

}
void 
syscall_munmap (struct intr_frame *f, uint32_t mapid)
{
  struct mmapped_file *mf = lookup_mmapped ( (mapid_t) mapid);
  if (mf == NULL)
    return;

  void *page;
  uint32_t *pd = thread_current ()->pagedir;
  void *kpage;
  int size = file_size (mf->file);

  for (page = mf->base_page; page - mf->base_page < size; page += PGSIZE)
  {
    if ( pagedir_is_dirty (pd, page))
    {
      off_t offset = (off_t) page - mf->base_page;
      file_seek (mf->file, offset);
      int bytes_to_write = size - offset > PGSIZE ? PGIZE : size - offset;
      file_write (mf->file, page, bytes_to_write);
    }
    kpage = pagedir_get_page (pd, page);
    frame_free (kpage);
    pagedir_clear_page (pd, page);
    page_free (page);
  }


  //remove from list of mmapped files
  list_remove (&mf->elem);
  free (mf);

  struct list_elem *e;
  struct list *mmap_list = &thread_current ()->mmapped_files;

  for (e = list_begin (mmap_list); e != list_end (mmap_list);
       e = list_next (e))
  {
    struct mmapped_file *mf = list_entry (e, struct mmapped_file, elem);
    if (mf->mapid == mapid){
      list_remove (e);
      free (mf);
      break;
    }
  }
}


static void
verify_uaddr (const void *uaddr)
{
  if (!is_user_vaddr (uaddr))
    thread_exit (); // Not user address
  if ( pagedir_get_page (thread_current ()->pagedir, uaddr) == NULL)
    thread_exit (); // Not mapped
}


/*check the start and end of buffer, and one address every PGSIZE
in between*/
static void
check_buffer_uaddr (const void *buf, int size)
{
  verify_uaddr (buf);
  int i;
  for(i = 1; i < (size -2 )/ PGSIZE; i++)
    verify_uaddr (buf + i*PGSIZE);
  verify_uaddr (buf + size - 1);
}
