#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

#define fdtable thread_current ()->files
inline static struct file* get_file(int fd) {
  return fdtable[fd];
}

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
static void exit (int status) {
  thread_exit ();
}

/* Runs the executable whose name is given in cmd_line, passing any 
  given arguments, and returns the new process's program id (pid).
  Must return pid -1, which otherwise should not be a valid pid, 
  if the program cannot load or run for any reason. */
// static int exec (const char *cmd_line) {}

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
  if (file == NULL) return false;
  return filesys_create (file, initial_size);
}

/* Deletes the file called file. Returns true if successful, false otherwise.*/ 
static bool remove (const char *file){
  if (file == NULL) return false;
  return filesys_remove (file);
}

/* Opens the file called file. Returns a nonnegative integer handle called a 
  "file descriptor" (fd), or -1 if the file could not be opened. */
static int open (const char *file){
  if (file == NULL) return -1;
  
  //find an open place in our fd table
  struct file **table = fdtable;
  int fd;
  for (fd = 2; fd < NUM_FD; fd++)
    if (*(table+fd) == NULL)
      break;
    
  if (fd == NUM_FD)
    return -1;
  
  table[fd] = filesys_open (file);
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
  if (buffer == NULL) return -1;
  if (fd == 1)
    return -1;

  if (fd == 0)
    return -1; //TODO: STDIN
  struct file *file = get_file (file);
  return file_read (file, buffer, size);
}

/* Writes size bytes from buffer to the open file fd. Returns the number of 
  bytes actually written, or -1 if the file could not be written. */
static int write (int fd, const void *buffer, unsigned size){
  struct file *file;
  
  if (fd == 1){
    putbuf (buffer, size);
    return size;
  }
  file = get_file (fd);
  if (file == NULL)
    return -1;
  return file_write (file, buffer, size);
}

/* Changes the next byte to be read or written in open file fd to position, 
  expressed in bytes from the beginning of the file. (Thus, a position of 
  0 is the file's start.) */
static void seek (int fd, unsigned position){
  file_seek (get_file (fd), position);
}

/* Returns the position of the next byte to be read or written in open file 
  fd, expressed in bytes from the beginning of the file. */
static unsigned tell (int fd){
  return file_tell (get_file (fd));
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly 
  closes all its open file descriptors, as if by calling this function 
  for each one. */
static void close (int fd){
  struct file *file = get_file (fd);
  if (fd < 2) return;
  if (file == NULL) return;
  file_close (file);
  fdtable[fd] = NULL;
}


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *args = f->esp; args++;
  int sys_call = *((int *)f->esp);
  int return_val = f->eax;
  
  switch (sys_call){
    case SYS_HALT    : halt (); break;
    case SYS_EXIT    : exit (args[0]); break;
    case SYS_EXEC    : break;                   //TODO
    case SYS_WAIT    : return_val = wait (args[0]); break; 
    case SYS_CREATE  : return_val = create ((char *)args[0], args[1]); break;
    case SYS_REMOVE  : return_val = remove ((char *)args[0]); break;
    case SYS_OPEN    : return_val = open ((char *)args[0]); break;
    case SYS_FILESIZE: break;                   //TODO
    case SYS_READ    : return_val = read (args[0], (char *)args[1], args[2]); break;
    case SYS_WRITE   : return_val = write (args[0], (char *)args[1], args[2]); break;
    case SYS_SEEK    : seek (args[0], args[1]); break; 
    case SYS_TELL    : return_val = tell (args[0]); break; 
    case SYS_CLOSE   : close (args[0]); break;
    default: printf ("Unknown system call\n");
             halt ();
  }
  
  //"return" the value back where it's expected
  f->eax=return_val;
}

/* Validate reading memory */
static void 
validate_read (char *buffer, unsigned size)
{
	int count = 0;
	int result;
	
	if (buffer + size > PHYS_BASE)
		thread_exit ();
	else
	{
		for (count = 0; count <= size, count ++)
		{
			result = get_user (buffer + count);
			if (result == -1)
			{
				thread_exit ();
				break;
			}	
		}
	}
}

/* Validate writing memory */
static void 
validate_write (uint8_t byte, void *buffer, unsigned size)
{
	int count;
	int result;
	
	if (buffer + size > PHYS_BASE)
		thread_exit ();
	else
	{
		for (count = 0; count <= size, count ++)
		{
			if (!put_user (buffer, byte))
			{
				thread_exit ();
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