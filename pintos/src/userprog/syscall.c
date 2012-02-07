#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <console.h>
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);
static void *translate_uaddr_to_kaddr (const void *vaddr);
static void check_buffer_uaddr (const void *buf, int size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t * k_esp = (uint32_t *) translate_uaddr_to_kaddr(f->esp);
  uint32_t syscall_number = (uint32_t) *(k_esp);
  uint32_t arg1 = (uint32_t) *(k_esp + 1);
  uint32_t arg2 = (uint32_t) *(k_esp + 2);
  uint32_t arg3 = (uint32_t) *(k_esp + 3);

  switch (syscall_number)
  {
      case SYS_HALT:
      case SYS_EXIT:
      case SYS_EXEC:
      {
        printf ("%s: exit(%d)\n", thread_current ()->name, arg1);
        f->eax = arg1;
        thread_exit ();
      }
      case SYS_WAIT:
      case SYS_CREATE:
      case SYS_REMOVE:
      case SYS_OPEN:
      case SYS_FILESIZE:
      case SYS_READ:
      case SYS_WRITE:
      {
        void *buf = (void *) arg2;
        int size = arg3;
        check_buffer_uaddr (buf, size);

        if (arg1 ==1) //fd 1
          putbuf (translate_uaddr_to_kaddr(buf), size);
        f->eax = size;
        break;
        
      }
      case SYS_SEEK:
      case SYS_TELL:
      case SYS_CLOSE:
      default:
      {
        printf ("system call!\n");
        thread_exit ();
      }  
      
  }
}

static void *
translate_uaddr_to_kaddr (const void *vaddr)
{
  ASSERT (is_user_vaddr (vaddr)); // Not user address
  uint32_t *kaddr = pagedir_get_page (thread_current ()->pagedir, vaddr);
  ASSERT (kaddr != NULL); // Not mapped
  return kaddr;
}

static void
check_buffer_uaddr (const void *buf, int size)
{
  int i;
  for (i = 0; i < size; ++i)
    ASSERT (translate_uaddr_to_kaddr (buf + i) != NULL);
}
