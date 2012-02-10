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

static void syscall_handler (struct intr_frame *);
static void *translate_uaddr_to_kaddr (const void *vaddr);
static void check_buffer_uaddr (const void *buf, int size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t * k_esp = (uint32_t *) translate_uaddr_to_kaddr(f->esp);
  uint32_t syscall_number = (uint32_t) *(k_esp); //TODO check all these addresses too
  uint32_t arg1 = (uint32_t) *(k_esp + 1);
  uint32_t arg2 = (uint32_t) *(k_esp + 2);
  uint32_t arg3 = (uint32_t) *(k_esp + 3);

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
  NOT_REACHED();
}

void syscall_exit(struct intr_frame *f, uint32_t status) {
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  f->eax = status;
  thread_exit ();
}

void syscall_exec(struct intr_frame *f, uint32_t file) {
  f->eax = process_execute ((char *) file);
}

void syscall_wait(struct intr_frame *f, uint32_t tid) {
  f->eax = process_wait ( (tid_t) tid);
}

void syscall_create(struct intr_frame *f, uint32_t file, uint32_t i_size)
{
  f->eax = filesys_create ( (char *) file, i_size);
}

void syscall_remove(struct intr_frame *f, uint32_t file) {
  f->eax = filesys_remove ( (char *) file);
}

void syscall_open(struct intr_frame *f, uint32_t file) {
  //TODO
}

void syscall_filesize(struct intr_frame *f, uint32_t fd) {
  //TODO
}

void syscall_read(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		  uint32_t length) {
  //TODO
}

void syscall_write(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length) {
  void *buf = (void *) buffer;
  int size = length;
  check_buffer_uaddr (buf, size);
  if (fd == 1) //fd 1
    putbuf (translate_uaddr_to_kaddr(buf), size);
  f->eax = size;
  //TODO if arg1 is not 1, ie console
}

void syscall_seek(struct intr_frame *f, uint32_t fd, uint32_t position) {
  //TODO
}

void syscall_tell(struct intr_frame *f, uint32_t fd) {
  //TODO
}

void syscall_close(struct intr_frame *f, uint32_t fd) {
  //TODO
}

static void *
translate_uaddr_to_kaddr (const void *vaddr)
{
  //TODO kill process instead of failing ASSERT
  ASSERT (is_user_vaddr (vaddr)); // Not user address
  uint32_t *kaddr = pagedir_get_page (thread_current ()->pagedir, vaddr);
  ASSERT (kaddr != NULL); // Not mapped
  return kaddr;
}

static void
check_buffer_uaddr (const void *buf, int size)
{
  //TODO kill process instead of failing ASSERT
  int i;
  for (i = 0; i < size; ++i)
    ASSERT (translate_uaddr_to_kaddr (buf + i) != NULL);
}
