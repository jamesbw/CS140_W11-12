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
  uint32_t arg1 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 4);
  uint32_t arg2 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 8);
  uint32_t arg3 = *(uint32_t *) translate_uaddr_to_kaddr(f->esp + 12);

  switch (syscall_number)
  {
      case SYS_HALT:
      {
        shutdown_power_off ();
        break;
      }
      case SYS_EXIT:
      {
        
        f->eax = arg1;

        //update exit code
        struct list_elem *e;
        struct process *p;
        for (e = list_begin (&process_list); e != list_end (&process_list);e = list_next (e))
        {
          p = list_entry (e, struct process, elem);
          if (p->tid == thread_current ()->tid)
          {
            p->exit_code = arg1;
            break;
          }
        }

        thread_exit ();
        break;
      }
      case SYS_EXEC:
      {
        void *k_arg1 = translate_uaddr_to_kaddr( (void *) arg1);
        f->eax = process_execute ( (char *) k_arg1);
        break;
      }
      case SYS_WAIT:
      {
        f->eax = process_wait ( (tid_t) arg1);
        break;
      }
      case SYS_CREATE:
      {
        void *k_arg1 = translate_uaddr_to_kaddr( (void *) arg1);
        f->eax = filesys_create ( (char *) k_arg1, arg2);
        break;
      }
      case SYS_REMOVE:
      {
        void *k_arg1 = translate_uaddr_to_kaddr( (void *) arg1);
        f->eax = filesys_remove ( (char *) k_arg1);
        break;
      }
      case SYS_OPEN:
      {
        void *k_arg1 = translate_uaddr_to_kaddr( (void *) arg1);
        lock_acquire (&filesys_lock);
        struct file *file = filesys_open ( (char *) k_arg1);
        if (file == NULL)
          f->eax = -1;
        else
        {
          struct file_wrapper *fw = wrap_file (file); 
          list_push_back (&thread_current ()->open_files, &fw->elem);   
          f->eax = fw->fd;
        }
        lock_release (&filesys_lock);
        break;
      }
      case SYS_FILESIZE:
      {
        struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
        lock_acquire (&filesys_lock);
        f->eax = file_length (fw->file);
        lock_release (&filesys_lock);
        break;
      }
      case SYS_READ:
      {
        void *buf = (void *) arg2;
        int size = arg3;
        check_buffer_uaddr (buf, size);
        char *k_buf = translate_uaddr_to_kaddr(buf);

        if ( arg1  == 0) // Read from keyboard
        {
          int count = 0;
          while (size - count > 0)
          {
            k_buf[count] = input_getc ();
            count ++;
          }
        }
        else
        {
          struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
          if (fw == NULL)
            f->eax =  -1;
          else
          {
            lock_acquire (&filesys_lock);
            f->eax = file_read (fw->file, k_buf, size);
            lock_release (&filesys_lock);
          }
        }
        break;
      }
      case SYS_WRITE:
      {
        void *buf = (void *) arg2;
        int size = arg3;
        check_buffer_uaddr (buf, size);
        void *k_buf = translate_uaddr_to_kaddr(buf);

        if (arg1 ==1) // Write to console
        { 
          putbuf ( k_buf, size);
          f->eax = size;
        }
        else
        {
          struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
          if (fw == NULL)
            f->eax =  -1;
          else
          {
            lock_acquire (&filesys_lock);
            f->eax = file_write (fw->file, k_buf, size);
            lock_release (&filesys_lock);
          }
        }
        break;        
      }
      case SYS_SEEK:
      {
        struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
        if (fw != NULL)
        {
          lock_acquire (&filesys_lock);
          file_seek (fw->file, arg2);
          lock_release (&filesys_lock);
        }
        break;
      }
      case SYS_TELL:
      {
        struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
        if (fw == NULL)
          f->eax =  -1;
        else
        {
          lock_acquire (&filesys_lock);
          f->eax = file_tell (fw->file);
          lock_release (&filesys_lock);
        }
        break;
      }
      case SYS_CLOSE:
      {
        struct file_wrapper *fw = lookup_fd ( (fd_t) arg1);
        if (fw != NULL)
        {
          list_remove (&fw->elem);
          lock_acquire (&filesys_lock);
          file_close (fw->file);
          lock_release (&filesys_lock);
          free (fw);
        }
        break;
      }
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
