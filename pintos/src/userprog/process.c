#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#include "vm/page.h"
#include "vm/frame.h"


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp, struct file **executable);
struct list process_list;

struct start_process_frame
{
  char * file_name;
  bool success;
  struct semaphore *sema_loaded;
  tid_t parent_tid;
};

struct lock filesys_lock;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  struct semaphore loaded;
  sema_init(&loaded, 0);

  struct start_process_frame spf;
  spf.file_name = fn_copy;
  spf.success = false;
  spf.sema_loaded = &loaded;
  spf.parent_tid = thread_current ()->tid;

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, &spf);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  else 
  {
    sema_down ( &loaded);
    if (! spf.success)
      return TID_ERROR;
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *spf_)
{
  struct start_process_frame *spf = spf_;

  char *file_name = spf->file_name;
  struct intr_frame if_;
  bool success = false;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;


  //create a struct process, add it to list_push_back
  struct process *new_process = malloc (sizeof (struct process));
  if (new_process != NULL)
  {
    new_process->parent_tid = spf->parent_tid;
    new_process->tid = thread_current ()->tid;
    new_process->finished = false;
    new_process->parent_finished = false;
    sema_init (&new_process->sema_finished, 0);
    new_process->exit_code = -1; 

    hash_init (&(new_process->supp_page_table), page_hash, page_less, NULL);
    thread_current ()->supp_page_table = &new_process->supp_page_table;

    lock_acquire(&process_lock);
    list_push_back ( &process_list, &new_process->elem);
    lock_release(&process_lock);
    lock_acquire ( &filesys_lock);
    success = load (file_name, &if_.eip, &if_.esp, &new_process->executable);
    lock_release ( &filesys_lock);

  }
  spf->success = success;

  sema_up (spf->sema_loaded);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  tid_t cur_tid = thread_current ()->tid;
  struct list_elem *e;
  lock_acquire(&process_lock);
  struct process *p = NULL;
  for (e = list_begin (&process_list); e != list_end (&process_list);
           e = list_next (e))
  {
    p = list_entry (e, struct process, elem);
    if (p->parent_tid == cur_tid && p->tid == child_tid)
      break;
  }

  if (e == list_end (&process_list))  { //not found
    lock_release(&process_lock);
    return -1;
  }
  lock_release(&process_lock);
  sema_down (&p->sema_finished);
  lock_acquire(&process_lock);
  list_remove (e);
  lock_release(&process_lock);
  int saved_exit_code = p->exit_code;
  free (p);
  return saved_exit_code;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;


  struct list_elem *e;


  //Release all locks if some are still held
  e = list_begin (&cur->locks_held);
  while (!list_empty (&cur->locks_held))
  {
    struct lock *l = list_entry (e, struct lock, elem);
    e = list_remove (e);
    lock_release (l);
  }

  
  //Close all open files
  e = list_begin (&cur->open_files);
  while (!list_empty (&cur->open_files))
  {
    struct file_wrapper *fw = list_entry (e, struct file_wrapper, elem);
    e = list_remove (e);
    lock_acquire (&filesys_lock);
    file_close (fw->file);
    lock_release (&filesys_lock);
    free (fw);
  }


  //Remove all mmapped files
  while (!list_empty (&cur->mmapped_files))
  {
    struct mmapped_file *mf = list_entry (list_begin (&cur->mmapped_files), struct mmapped_file, elem);
    process_munmap ( mf->mapid); 
  }

  //Free swap

  //Free all frames

  //Frre all supp
  page_free_supp_page_table (); 

  // Update the process list
  lock_acquire(&process_lock);
  e = list_begin (&process_list);
  struct process *p = NULL;
  while (e != list_end (&process_list))
  {
    p = list_entry (e, struct process, elem);
    if (p->parent_tid == cur->tid)
    {
      if (p->finished)
      {
        e = list_remove (e);
        free (p);
      }
      else
      {
        p->parent_finished = true;
        e = list_next (e);
      }
    }
    else if (p->tid == cur->tid)
    {
      if (p->parent_finished)
      {
        e = list_remove (e);
        free (p);
      }
      else
      {
        p->finished = true;
        file_close(p->executable);
        printf ("%s: exit(%d)\n", cur->name, p->exit_code);
        sema_up (&p->sema_finished);
        e = list_next (e);
      }
    }
    else
      e = list_next (e);
  }
  lock_release(&process_lock);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *command_line);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static fd_t allocate_fd (void);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp, struct file **executable) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
  
  char *first_space = strchr (file_name, ' ');
  while (first_space == file_name) {
    file_name++;
    first_space = strchr(file_name, ' ');
  }
  if(first_space != NULL){
    *first_space = '\0'; // shorten file_name to contain only executable name
  } 

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  file_deny_write (file);
  *executable = file;

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if(first_space != NULL){
    *first_space = ' '; // restore original file_name
  } 
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;


      // create new page in supp table
      page_insert_executable (upage, file, ofs, page_read_bytes, writable);
      ofs += PGSIZE;


      /* Get a page of memory. */
      // uint8_t *kpage = palloc_get_page (PAL_USER);
      // if (kpage == NULL)
      //   return false;

      // /* Load this page. */
      // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
      //   {
      //     palloc_free_page (kpage);
      //     return false; 
      //   }
      // memset (kpage + page_read_bytes, 0, page_zero_bytes);

      // /* Add the page to the process's address space. */
      // if (!install_page (upage, kpage, writable)) 
      //   {
      //     palloc_free_page (kpage);
      //     return false; 
      //   }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *command_line)
{
  uint8_t *kpage;
  bool success = false;
  uint32_t offset = 0;
  // kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  kpage = frame_allocate (*esp);
  if (kpage != NULL) 
    {
      memset (kpage, 0, PGSIZE);
      uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
      success = install_page (upage, kpage, true);
      // success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){

        page_insert_zero (upage);
        char *cl_copy = palloc_get_page (0);
        if (cl_copy == NULL)
          return false;
        strlcpy (cl_copy, command_line, PGSIZE);

        uint32_t argc = 0;
        char *token, *save_ptr;
        *esp = PHYS_BASE;


        //pushing argument strings
        for (token = strtok_r (cl_copy, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr))
        {
          argc ++ ;
          *esp -= strlen(token) + 1;
      	  offset += strlen(token) + 1;
      	  // if (offset >= 4096) 
         //  {
      	  //   palloc_free_page(cl_copy);
      	  //   palloc_free_page(kpage);
      	  //   return false;
      	  // }
          memcpy(*esp, token, strlen(token) + 1);          
        }
        char *end_of_args = *esp;

        //word align
        int word_align_length = (uint32_t) *esp % 4;
        *esp -= word_align_length;
      	offset += word_align_length;
      	// if (offset >= 4096) 
       //  {
      	//   palloc_free_page(cl_copy);
      	//   palloc_free_page(kpage);
      	//   return false;
      	// }
      	memset(*esp, 0, word_align_length);

        //push sentinel
        *esp -= 4;
      	offset += 4;
      	// if (offset >= 4096) 
       //  {
      	//   palloc_free_page(cl_copy);
      	//   palloc_free_page(kpage);
      	//   return false;
      	// }
        *((uint32_t *) *esp) = 0;

        //pushing argv elements
        token = end_of_args;
        uint32_t count =0;
        while(count < argc)
        {
          count ++;
          *esp -=4;
      	  offset += 4;
      	  // if (offset >= 4096) 
         //  {
      	  //   palloc_free_page(cl_copy);
      	  //   palloc_free_page(kpage);
      	  //   return false;
      	  // }
          *((char **) *esp) = token;
          token = strchr(token,'\0') + 1;
        }

        //pushing &argv
        *esp -=4;
      	offset += 4;
      	// if (offset >= 4096) 
       //  {
      	//   palloc_free_page(cl_copy);
      	//   palloc_free_page(kpage);
      	//   return false;
      	// }
        *((char ***) *esp )=  *esp + 4;

        //pusing argc
        *esp -= 4;
      	offset += 4;
      	// if (offset >= 4096) 
       //  {
      	//   palloc_free_page(cl_copy);
      	//   palloc_free_page(kpage);
      	//   return false;
      	// }
        *((uint32_t *) *esp) = argc;

        //pushing fake return address
        *esp -=4;
      	offset += 4;
      	// if (offset >= 4096) 
       //  {
      	//   palloc_free_page(cl_copy);
      	//   palloc_free_page(kpage);
      	//   return false;
      	// }
        memset(*esp, 0, 4);

        palloc_free_page (cl_copy);
      }
      else
        // palloc_free_page (kpage);
        frame_free (kpage);
    }
  return success;
}

 // Adds a mapping from user virtual address UPAGE to kernel
 //   virtual address KPAGE to the page table.
 //   If WRITABLE is true, the user process may modify the page;
 //   otherwise, it is read-only.
 //   UPAGE must not already be mapped.
 //   KPAGE should probably be a page obtained from the user pool
 //   with palloc_get_page().
 //   Returns true on success, false if UPAGE is already mapped or
 //   if memory allocation fails. 
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}


// Wraps a struct file into a struct with a file descriptor
// and a list_elem so that it can be inserted into a list.
struct file_wrapper *
wrap_file (struct file *file)
{
  struct file_wrapper *fw = malloc (sizeof (struct file_wrapper));
  if (fw == NULL) //couldn't allocate memory
    thread_exit ();
  fw->file = file;
  fw->fd = allocate_fd ();
  return fw;
}

// Returns a struct file wrapper for a file descriptor.
// If the current thread doesn't have this fd, returns NULL.
struct file_wrapper *
lookup_fd ( fd_t fd)
{
  struct thread *cur = thread_current ();
  struct file_wrapper *fw = NULL;
  struct list_elem *e;

  for (e = list_begin (&cur->open_files); e != list_end (&cur->open_files);e = list_next (e))
  {
    fw = list_entry (e, struct file_wrapper, elem);
    if (fw->fd == fd)
      break;
  }
  return fw;
}

/* Returns a fd to use for a new open file. */
// Filesys lock must be acquired while this is called.
static fd_t
allocate_fd (void) 
{

  ASSERT (lock_held_by_current_thread (&filesys_lock));

  static fd_t next_fd = 2; // 0 and 1 are reserved
  fd_t fd;

  fd = next_fd++;

  return fd;
}

struct mmapped_file * 
lookup_mmapped ( mapid_t mapid)
{
  struct thread *cur = thread_current ();
  struct mmapped_file *mf = NULL;
  struct list_elem *e;

  for (e = list_begin (&cur->mmapped_files); e != list_end (&cur->mmapped_files);e = list_next (e))
  {
    mf = list_entry (e, struct mmapped_file, elem);
    if (mf->mapid == mapid)
      break;
  }
  return mf;
}

void
process_munmap (mapid_t mapid)
{
  struct mmapped_file *mf = lookup_mmapped ( (mapid_t) mapid);
  if (mf == NULL)
    return;

  void *page;
  uint32_t *pd = thread_current ()->pagedir;
  void *kpage;

  lock_acquire (&filesys_lock);
  int size = file_length (mf->file);
  lock_release (&filesys_lock);

  for (page = mf->base_page; page - mf->base_page < size; page += PGSIZE)
  {
    if ( pagedir_is_dirty (pd, page))
    {
      off_t offset = (off_t) (page - mf->base_page);
      int bytes_to_write = size - offset > PGSIZE ? PGSIZE : size - offset;
      lock_acquire (&filesys_lock);
      file_seek (mf->file, offset);
      file_write (mf->file, page, bytes_to_write);
      lock_release (&filesys_lock);
    }
    kpage = pagedir_get_page (pd, page);
    if (kpage) // page may not be in physical memory
    {
      frame_free (kpage);
      pagedir_clear_page (pd, page);
    }
    page_free ( thread_current (), page);
  }


  //remove from list of mmapped files
  list_remove (&mf->elem);
  free (mf);
}



