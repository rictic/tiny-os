#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filesys_lock;

void syscall_init (void);
void exit (int);
void close (int);

#define fdtable thread_current ()->files

#endif /* userprog/syscall.h */
