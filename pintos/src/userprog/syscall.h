#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

// Prototypes for each system call called by the handler.

void syscall_halt (void) NO_RETURN;
void syscall_exit (struct intr_frame *f, uint32_t status) NO_RETURN;
void syscall_exec (struct intr_frame *f, uint32_t file);
void syscall_wait (struct intr_frame *f, uint32_t tid);
void syscall_create (struct intr_frame *f, uint32_t file, uint32_t i_size);
void syscall_remove (struct intr_frame *f, uint32_t file);
void syscall_open (struct intr_frame *f, uint32_t file);
void syscall_filesize (struct intr_frame *f, uint32_t fd);
void syscall_read (struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_write(struct intr_frame *f, uint32_t fd, uint32_t buffer,
		   uint32_t length); 
void syscall_seek (struct intr_frame *f, uint32_t fd, uint32_t position);
void syscall_tell (struct intr_frame *f, uint32_t fd);
void syscall_close (struct intr_frame *f, uint32_t fd);

#endif /* userprog/syscall.h */
