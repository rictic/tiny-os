#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#define fdtable thread_current ()->files
inline static struct file* get_file(int fd) {
  if ((fd < 0) || (fd >= NUM_FD)) exit(-1);
  return fdtable[fd];
}

static struct lock filesys_lock;

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void validate_read (char *buffer, unsigned size);
static void validate_write (uint8_t byte, void *buffer, unsigned size);

/* Terminates Pintos by calling power_off() (declared in "threads/init.h"). 
 This should be seldom used, because you lose some information about possible
 deadlock situations, etc.  */
static void halt (void) {
  power_off ();
}

/* Terminates the current user program, returning status to the kernel. If 
  the process's parent waits for it (see below), this is the status that 
  will be returned. Conventionally, a status of 0 indicates success and 
  nonzero values indicate errors. */
void exit (int status) {
  struct thread *t = thread_current ();
  // if the parent isn't dead
  if (t->parent != NULL){
    lock_acquire (&t->parent->children_lock);
    //the parent could have died while acquiring the lock
    if (t->parent != NULL) {
      struct dead_thread *dead = malloc (sizeof(struct dead_thread));

      dead->tid = t->tid;
      dead->status = THREAD_DEAD;
      dead->exit_code = status;
      list_remove(&t->child_elem);
      list_push_back(&t->parent->children, &dead->child_elem);
    }
    lock_release (&t->parent->children_lock);
  }
  printf("%s: exit(%d)\n", t->name, status);
  
  thread_exit ();
}

/* Runs the executable whose name is given in cmd_line, passing any 
  given arguments, and returns the new process's program id (pid).
  Must return pid -1, which otherwise should not be a valid pid, 
  if the program cannot load or run for any reason. */
static int exec (const char *cmd_line) {
  //TODO: validate cmd_line
  lock_acquire (&filesys_lock);
  tid_t tid = process_execute (cmd_line);
  lock_release (&filesys_lock);
  if (tid == TID_ERROR)
    return -1;
  return tid;
}

/* If process pid is still alive, waits until it dies. Then, returns
 the status that pid passed to exit, or -1 if pid was terminated by 
 the kernel (e.g. killed due to an exception). If pid does not refer 
 to a child of the calling thread, or if wait has already been 
 successfully called for the given pid, returns -1 immediately, 
 without waiting. */
static int wait (int pid){
  return process_wait (pid);
}

/* Creates a new file called file initially initial_size bytes in size.
 Returns true if successful, false otherwise. */
static bool create (const char *file, unsigned initial_size){
  //TODO: validate file
  bool result;
  if (file == NULL) exit(-1);
  lock_acquire (&filesys_lock);
  result = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return result;
}

/* Deletes the file called file. Returns true if successful, false otherwise.*/ 
static bool remove (const char *file){
  //TODO: validate file
  bool result;
  if (file == NULL) return false;
  lock_acquire (&filesys_lock);
  result = filesys_remove (file);
  lock_release (&filesys_lock);
  return result;
}

/* Opens the file called file. Returns a nonnegative integer handle called a 
  "file descriptor" (fd), or -1 if the file could not be opened. */
static int open (const char *file){
  //TODO: validate file
  if (file == NULL) exit(-1);
  
  //find an open place in our fd table
  struct file **table = fdtable;
  int fd;
  for (fd = 2; fd < NUM_FD; fd++)
    if (*(table+fd) == NULL)
      break;
    
  if (fd == NUM_FD)
    return -1;
  
  lock_acquire (&filesys_lock);
  table[fd] = filesys_open (file);
  lock_release (&filesys_lock);
  if (table[fd] == NULL) return -1;
  return fd;
}

/* Returns the size, in bytes, of the file open as fd. */
// static int filesize (int fd){}

/* Reads size bytes from the file open as fd into buffer. Returns the number of
 bytes actually read (0 at end of file), or -1 if the file could not be read 
 (due to a condition other than end of file). Fd 0 reads from the keyboard 
 using input_getc(). */
static int read (int fd, void *buffer, unsigned size){
  //TODO: validate buffer
  int result;
  if (buffer == NULL) return -1;
  if (fd == 1)
    return -1;

  if (fd == 0)
    return -1; //TODO: STDIN
  struct file *file = get_file (fd);
  lock_acquire (&filesys_lock);
  result = file_read (file, buffer, size);
  lock_release (&filesys_lock);
  return result;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of 
  bytes actually written, or -1 if the file could not be written. */
static int write (int fd, const void *buffer, unsigned size){
  //TODO: validate buffer
  struct file *file;
  int result;
  
  if (fd == 1){
    putbuf (buffer, size);
    return size;
  }
  file = get_file (fd);
  if (file == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  result = file_write (file, buffer, size);
  lock_release (&filesys_lock);
  return result;
}

/* Changes the next byte to be read or written in open file fd to position, 
  expressed in bytes from the beginning of the file. (Thus, a position of 
  0 is the file's start.) */
static void seek (int fd, unsigned position){
  struct file * f = get_file (fd);
  lock_acquire (&filesys_lock);
  file_seek (f, position);
  lock_release (&filesys_lock);
}

/* Returns the position of the next byte to be read or written in open file 
  fd, expressed in bytes from the beginning of the file. */
static unsigned tell (int fd){
  unsigned result;
  struct file * f = get_file (fd);
  lock_acquire (&filesys_lock);
  result = file_tell (f);
  lock_release (&filesys_lock);
  return result;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly 
  closes all its open file descriptors, as if by calling this function 
  for each one. */
void close (int fd){
  struct file *file = get_file (fd);
  if (fd < 2) return;
  if (file == NULL) return;
  lock_acquire (&filesys_lock);
  file_close (file);
  lock_release (&filesys_lock);
  fdtable[fd] = NULL;
}


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  //TODO: validate f->esp
  int *args = f->esp; args++;
  int return_val = f->eax;
	
  validate_read (f->esp, 1);
  
  int sys_call = *((int *)f->esp);

  switch (sys_call){
    case SYS_HALT    : halt (); break;
    case SYS_EXIT    : validate_read (args, 1); exit (args[0]); break;
    case SYS_EXEC    : validate_read (args, 1); return_val = exec ((char *)args[0]); break;
    case SYS_WAIT    : validate_read (args, 1); return_val = wait (args[0]); break; 
    case SYS_CREATE  : validate_read (args, 2); return_val = create ((char *)args[0], args[1]); break;
    case SYS_REMOVE  : validate_read (args, 1); return_val = remove ((char *)args[0]); break;
    case SYS_OPEN    : validate_read (args, 1); return_val = open ((char *)args[0]); break;
    case SYS_FILESIZE: break;                                         //TODO
    case SYS_READ    : validate_read (args, 3); return_val = read (args[0], (char *)args[1], args[2]); break;
    case SYS_WRITE   : validate_read (args, 3); return_val = write (args[0], (char *)args[1], args[2]); break;
    case SYS_SEEK    : validate_read (args, 2); seek (args[0], args[1]); break; 
    case SYS_TELL    : validate_read (args, 1); return_val = tell (args[0]); break; 
    case SYS_CLOSE   : validate_read (args, 1); close (args[0]); break;
    default: exit(-1);
    /*default: printf ("Unknown system call\n");
             halt ();*/
  }

  //"return" the value back where it's expected
  f->eax=return_val;
}

/* Validate reading memory */
static void 
validate_read (char *buffer, unsigned size)
{
	unsigned count = 0;
	
	if (buffer + size >= (size_t)PHYS_BASE)
		exit(-1);
		//thread_exit ();
	else
	{	
		for (count = 0; count <= size; count ++)
		{
			if (get_user (buffer + count) == -1)
			{
				exit(-1);
				//thread_exit ();
				break;
			}	
		}
	}
}

/* Validate writing memory */
static void 
validate_write (uint8_t byte, void *buffer, unsigned size)
{
	unsigned count;
	
	if (buffer + size >= (size_t)PHYS_BASE)
		exit(-1);
		//thread_exit ();
	else
	{
		for (count = 0; count <= size; count ++)
		{
			if (!put_user (buffer, byte))
			{
				exit(-1);
				//thread_exit ();
				break;
			}	
		}
	}	
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool 
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "r" (byte));
  return error_code != -1;
}
