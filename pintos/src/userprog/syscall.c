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

static void syscall_handler (struct intr_frame *);
static void *translate_uaddr_to_kaddr (const void *vaddr);
static void check_buffer_uaddr (const void *buf, int size);

// Prototypes for each system call called by the handler.
void syscall_halt (void);
void syscall_exit (struct intr_frame *f, uint32_t status);
void syscall_exec (struct intr_frame *f, uint32_t file);
void syscall_wait (struct intr_frame *f, uint32_t tid);
void syscall_create (struct intr_frame *f, uint32_t file, uint32_t i_size);
void syscall_remove (struct intr_frame *f, uint32_t file);
void syscall_open (struct intr_frame *f, uint32_t fd);
void syscall_filesize (struct intr_frame *f, uint32_t fd);
void syscall_read (struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_write(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_seek (struct intr_frame *f, uint32_t fd, uint32_t position);
void syscall_tell (struct intr_frame *f, uint32_t fd);
void syscall_close (struct intr_frame *f, uint32_t fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t * k_esp = (uint32_t *) translate_uaddr_to_kaddr(f->esp);
  uint32_t syscall_number = (uint32_t) *(k_esp); 
  uint32_t arg1 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 4);
  uint32_t arg2 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 8);
  uint32_t arg3 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 12);

  switch (syscall_number)
  {
      case SYS_HALT:
      {
	syscall_halt();
        break;
      }
      case SYS_EXIT:
      {
	syscall_exit(f, arg1);
	break;
      }
      case SYS_EXEC:
      {
	syscall_exec(f, arg1);
        break;
      }
      case SYS_WAIT:
      {
	syscall_wait(f, arg1);
        break;
      }
      case SYS_CREATE:
      {
	syscall_create(f, arg1, arg2);
        break;
      }
      case SYS_REMOVE:
      {
	syscall_remove(f, arg1);
        break;
      }
      case SYS_OPEN:
      {
	syscall_open(f, arg1);
	break;
      }
      case SYS_FILESIZE:
      {
	syscall_filesize(f, arg1);
	break;
      }
      case SYS_READ:
      {
	syscall_read(f, arg1, arg2, arg3);
	break;
      }
      case SYS_WRITE:
      {
	syscall_write(f, arg1, arg2, arg3);
        break;        
      }
      case SYS_SEEK:
      {
	syscall_seek(f, arg1, arg2);
	break;
      }
      case SYS_TELL:
      {
	syscall_tell(f, arg1);
	break;
      }
      case SYS_CLOSE:
      {
	syscall_close(f, arg1);
	break;
      }
      default:
      {
        printf ("system call!\n");
        thread_exit ();
	break;
      }  
      
  }
}

void syscall_halt(void) {
  shutdown_power_off ();
}

void syscall_exit(struct intr_frame *f, uint32_t status) {
  f->eax = status;
  //Update exit code
  struct list_elem *e;
  struct process *p;
  for (e = list_begin(&process_list); e != list_end(&process_list); e =
	 list_next (e)) {
    p = list_entry(e, struct process, elem);
    if (p->tid == thread_current()->tid) {
      p->exit_code = status;
      break;
    }
  }
  thread_exit ();
}

void syscall_exec(struct intr_frame *f, uint32_t file) {
  void *k_file = translate_uaddr_to_kaddr((void *) file);
  f->eax = process_execute ((char *) k_file);
}

void syscall_wait(struct intr_frame *f, uint32_t tid) {
  f->eax = process_wait ( (tid_t) tid);
}

void syscall_create(struct intr_frame *f, uint32_t file, uint32_t i_size)
{
  void *k_file = translate_uaddr_to_kaddr( (void *) file);
  f->eax = filesys_create ( (char *) k_file, i_size);
}

void syscall_remove(struct intr_frame *f, uint32_t file) {
  void *k_file = translate_uaddr_to_kaddr( (void *) file);
  f->eax = filesys_remove ( (char *) k_file);
}

void syscall_open(struct intr_frame *f, uint32_t fd) {
  void *k_fd = translate_uaddr_to_kaddr( (void *) fd);
  lock_acquire (&filesys_lock);
  struct file *file = filesys_open ( (char *) k_fd);
  if (file == NULL) {
    f->eax = -1;
  } else {
    struct file_wrapper *fw = wrap_file (file); 
    list_push_back (&thread_current ()->open_files, &fw->elem);   
    f->eax = fw->fd;
  }
  lock_release (&filesys_lock);
}

void syscall_filesize(struct intr_frame *f, uint32_t fd) {
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  lock_acquire (&filesys_lock);
  f->eax = file_length (fw->file);
  lock_release (&filesys_lock);
}

void syscall_read(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		  uint32_t length) {
  void *buf = (void *) buffer;
  int size = length;
  check_buffer_uaddr (buf, size);
  char *k_buf = translate_uaddr_to_kaddr(buf);
  
  if (fd  == 0) { //Read from Keyboard
    int count = 0;
    while (size - count > 0) {
      k_buf[count] = input_getc ();
      count ++;
    }
  } else {
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL) {
      f->eax =  -1;
    } else {
      lock_acquire (&filesys_lock);
      f->eax = file_read (fw->file, k_buf, size);
      lock_release (&filesys_lock);
    }
  }
}

void syscall_write(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length) {
  void *buf = (void *) buffer;
  int size = length;
  check_buffer_uaddr (buf, size);
  void *k_buf = translate_uaddr_to_kaddr(buf);
  int buf_chunk = 200;
  if (fd == 1) {  //Write to console 
    while (size > buf_chunk) {
      putbuf(k_buf, buf_chunk);
      size -= buf_chunk;
    }
    putbuf(k_buf, size);
    size = length;
    f->eax = size;
  } else {
    struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
    if (fw == NULL) {
      f->eax =  -1;
    } else {
      lock_acquire (&filesys_lock);
      f->eax = file_write (fw->file, k_buf, size);
      lock_release (&filesys_lock);
    }
  }
}

void syscall_seek(struct intr_frame *f UNUSED, uint32_t fd, uint32_t position) {
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) {  //TODO Dealing with illegal files
    lock_acquire (&filesys_lock);
    file_seek (fw->file, position);
    lock_release (&filesys_lock);
  }
}

void syscall_tell(struct intr_frame *f, uint32_t fd) {
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw == NULL) {
    f->eax =  -1;
  } else {
    lock_acquire (&filesys_lock);
    f->eax = file_tell (fw->file);
    lock_release (&filesys_lock);
  }
}

void syscall_close(struct intr_frame *f UNUSED, uint32_t fd) {
  struct file_wrapper *fw = lookup_fd ( (fd_t) fd);
  if (fw != NULL) {
    list_remove (&fw->elem);
    lock_acquire (&filesys_lock);
    file_close (fw->file);
    lock_release (&filesys_lock);
    free (fw);
  }
}

static void *
translate_uaddr_to_kaddr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr))
    thread_exit (); // Not user address
  uint32_t *kaddr = pagedir_get_page (thread_current ()->pagedir, vaddr);
  if (kaddr == NULL)
    thread_exit (); // Not mapped
  return kaddr;
}

static void
check_buffer_uaddr (const void *buf, int size)
{
  int i;
  for (i = 0; i < size; ++i)
    if (translate_uaddr_to_kaddr (buf + i) == NULL)
      thread_exit ();
}
