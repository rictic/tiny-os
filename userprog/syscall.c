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



static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void validate_read (const char *buffer, unsigned size);
static void validate_write (char *from, char *user_to, unsigned size, bool malloc_buffer);
static void validate_string (const char *string);

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
  t->exit_code = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit ();
  NOT_REACHED ();
}

/* Runs the executable whose name is given in cmd_line, passing any 
  given arguments, and returns the new process's program id (pid).
  Must return pid -1, which otherwise should not be a valid pid, 
  if the program cannot load or run for any reason. */
static int exec (const char *cmd_line) {
  validate_string (cmd_line);
  tid_t tid = process_execute (cmd_line);
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
  validate_string (file);
  bool result;
  if (file == NULL) exit(-1);
  lock_acquire (&filesys_lock);
  result = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return result;
}

/* Deletes the file called file. Returns true if successful, false otherwise.*/ 
static bool remove (const char *file){
  validate_string (file);
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
  validate_string (file);
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


/* Reads size bytes from the file open as fd into buffer. Returns the number of
 bytes actually read (0 at end of file), or -1 if the file could not be read 
 (due to a condition other than end of file). Fd 0 reads from the keyboard 
 using input_getc(). */
static int read (int fd, void *buffer, unsigned size){
  //TODO: validate buffer
  int bytes_read;
  char * localbuff;
  if (buffer == NULL) return -1;
  if (fd == 1)
    return -1;
  if (fd == 0)
    return -1; //TODO: STDIN
    
  struct file *file = get_file (fd);
  localbuff = malloc (size);
  lock_acquire (&filesys_lock);
  bytes_read = file_read (file, localbuff, size);
  lock_release (&filesys_lock);
  validate_write (localbuff, buffer, bytes_read, true);
  free (localbuff);
  return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of 
  bytes actually written, or -1 if the file could not be written. */
static int write (int fd, const void *buffer, unsigned size){
  validate_read(buffer, size);
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

/* Returns the size, in bytes, of the file open as fd. */
static int filesize (int fd){
  int result;
  struct file * f = get_file (fd);
  lock_acquire (&filesys_lock);
  result = file_length (f);
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

/* Mapping the file in fd file descriptor to virtual address addr.
   Returning the unique int if successful, otherwise returning -1. */
static int mmap (int fd, void *addr)
{
	int mapping = -1;
	
	/* File descriptors 0 and 1 are not mappable. */
	if (fd < 2) return mapping;
	
    lock_acquire (&filesys_lock);

	struct file *file = get_file (fd);
	
	/* Failed if file is NULL. */
	if (file == NULL) 
	{
		lock_release (&filesys_lock);
		return mapping;
	}	

	/* Failed if file is 0 length, or virtual address addr is in page 0, or addr is not page-aligned. */
	uint32_t read_bytes = file_length(file);
	if (read_bytes == 0 || addr < PGSIZE || (((uint32_t)addr) & 0x00000fff) != 0)
	{
		lock_release (&filesys_lock);
		return mapping;
	}	
	
	/* Failed if the range of pages mapped overlaps any existing set of mapping pages. 
	   (Stack validation not implimented yet!) */
	if (!validate_free_page (addr, read_bytes))
	{
		lock_release (&filesys_lock);
		return mapping;
	}	
	
	/* Otherwise, fulfill the file mapping. */
	off_t ofs = 0;
	mapping = addr; // Use the virtual address as mapping id.
	while (read_bytes > 0) 
    {		
		/* Calculate how to fill this page.
		We will read PAGE_READ_BYTES bytes from FILE
		and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		struct file_page *file_page = malloc (sizeof (struct file_page));
		file_page->type = FILE;
		file_page->virtual_page = (uint32_t)addr;
		file_page->source_file = file;
		file_page->offset = ofs;
		file_page->zero_after = page_read_bytes;
	    add_lazy_page (file_page);

		/* Advance. */
		read_bytes -= page_read_bytes;
		ofs += PGSIZE;
		addr += PGSIZE;
    }
	file_seek (file, ofs);
	
    lock_release (&filesys_lock);
	return mapping;
}

/* Unmaps the mapping designated by int mapping. */
static void munmap (int mapping)
{
	if ((mapping & 0x00000fff) == 0)
	{
		struct file_page *file_page = (struct file_page*) find_lazy_page(mapping);
		
		if (file_page != NULL)
		{
		    lock_acquire (&filesys_lock);

			struct thread *cur = thread_current();
			struct file *file = file_page->source_file;
			uint32_t read_bytes = file_length(file);
			uint32_t addr = mapping;
			void *kpage;

			if (read_bytes != 0)
			{	
				/* Get number of pages for this file. */
				size_t num_of_pages = read_bytes / PGSIZE;
				if (read_bytes % PGSIZE != 0)
					num_of_pages++;
				
				int i;
				for(i = 0; i < num_of_pages; i++)
				{
					kpage = pagedir_get_page (cur->pagedir, addr);
					if (kpage != NULL)
					{
						if (pagedir_is_dirty (cur->pagedir, addr))
							file_write_at (file, addr, file_page->zero_after, file_page->offset);
						ft_free_page (kpage);
					}	
						  
					hash_delete (&cur->sup_pagetable, &file_page->elem);
					free(file_page);
					addr += PGSIZE;
					file_page = (struct file_page*) find_lazy_page(addr);
				}	  			  
			}
		    lock_release (&filesys_lock);
		}		
	}
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
  int *args = f->esp; args++;
  int return_val = f->eax;
	
  validate_read (f->esp, 1);
  
  int sys_call = *((int *)f->esp);

  switch (sys_call){
    case SYS_HALT    : halt (); break;
    case SYS_EXIT    : validate_read ((char *)args, 1); exit (args[0]); break;
    case SYS_EXEC    : validate_read ((char *)args, 1); return_val = exec ((char *)args[0]); break;
    case SYS_WAIT    : validate_read ((char *)args, 1); return_val = wait (args[0]); break; 
    case SYS_CREATE  : validate_read ((char *)args, 2); return_val = create ((char *)args[0], args[1]); break;
    case SYS_REMOVE  : validate_read ((char *)args, 1); return_val = remove ((char *)args[0]); break;
    case SYS_OPEN    : validate_read ((char *)args, 1); return_val = open ((char *)args[0]); break;
    case SYS_FILESIZE: validate_read ((char *)args, 1); return_val = filesize (args[0]); break;
    case SYS_READ    : validate_read ((char *)args, 3); return_val = read (args[0], (char *)args[1], args[2]); break;
    case SYS_WRITE   : validate_read ((char *)args, 3); return_val = write (args[0], (char *)args[1], args[2]); break;
    case SYS_SEEK    : validate_read ((char *)args, 2); seek (args[0], args[1]); break; 
    case SYS_TELL    : validate_read ((char *)args, 1); return_val = tell (args[0]); break; 
    case SYS_CLOSE   : validate_read ((char *)args, 1); close (args[0]); break;
    
    /* Project 3 and optionally project 4. */
    case SYS_MMAP    : validate_read ((char *)args, 2); return_val = mmap (args[0], args[1]); break; 
    case SYS_MUNMAP  : validate_read ((char *)args, 1); munmap (args[0]); break; 

    default: exit(-1);
  }
  
  //"return" the value back as though this were a function call

  f->eax=return_val;
}

static void
validate_string (const char * string) {
  size_t i;
  int val = -1;
  if (string == NULL) exit(-1);
  for(i = 0;val != 0;i++){
	if (string + i >= (char *)PHYS_BASE) exit(-1);
    val = get_user(string+i);
    if (val == -1) exit(-1);
  }
}

/* Validate reading from user memory */
static void 
validate_read (const char *buffer, unsigned size)
{
	unsigned count = 0;
	
	if (buffer + size >= (char *)PHYS_BASE)
		exit(-1);
	for (count = 0; count < size; count ++)
		if (get_user (buffer + count) == -1)
			exit(-1);
}

/* Validate writing to user memory */
static void 
validate_write (char *from, char *user_to, unsigned size, bool malloc_buffer)
{
	unsigned count;
	
	if (user_to + size >= (char *)PHYS_BASE)
	{
		if (malloc_buffer) free(from);
		exit(-1);		
	}	
	for (count = 0; count < size; count ++) {
		if (!put_user (user_to+count, *(from+count)))
		{
			if (malloc_buffer) free(from);			
			exit(-1);			
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
