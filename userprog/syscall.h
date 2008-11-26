#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <syscall.h>

struct lock filesys_lock;

void syscall_init (void);
void exit (int);
void close (int);

#endif /* userprog/syscall.h */
