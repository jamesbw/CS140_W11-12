#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct file_wrapper * lookup_fd ( fd_t fd)


extern struct list process_list;
extern struct lock filesys_lock;

typedef int fd_t;

struct process
{
    tid_t parent_tid;
    tid_t tid;
    bool finished;
    bool parent_finished;
    struct semaphore sema_finished;
    int exit_code;
    struct list_elem elem;
};

struct file_wrapper
{
    fd_t fd;
    struct file *file;
    struct list_elem elem;
};

struct file_wrapper *wrap_file (struct file *file);

#endif /* userprog/process.h */
