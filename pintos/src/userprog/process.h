#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

extern struct list process_list;

struct process
{
    tid_t parent_tid;
    tid_t tid;
    bool parent_finished;
    bool finished;
    struct semaphore sema_finished;
    uint32_t exit_code;
    struct list_elem elem;
};

#endif /* userprog/process.h */
