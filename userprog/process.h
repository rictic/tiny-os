#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_STACK_SIZE 2000 //in pages
#define STACK_BOTTOM PHYS_BASE - (PGSIZE * MAX_STACK_SIZE)

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
