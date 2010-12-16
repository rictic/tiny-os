#include "lib/limits.h"
#include "lib/round.h"
#include "lib/stddef.h"
#include "lib/string.h"
#include "lib/stdio.h"
#include "lib/debug.h"
#include "lib/kernel/bitmap.h"
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "common.h"

bool bitmap_write (const struct bitmap *b, struct file *file);
void free_map_release (disk_sector_t sector, size_t cnt);
void * pagedir_get_page (uint32_t *pd, const void *uaddr);
bool pagedir_is_dirty (uint32_t *pd, const void *vpage);
void exit (int status);
void close (int fd);
void ft_init (void);
void swap_init (void);

/* vm/swap.c */
/* List of free swap slots. */
struct list free_swap_list;
/* Lock for the swap table. */
struct lock swap_lock;
/* The disk that contains the file system. */
struct disk *swap_disk;

/* vm/frame.c */
/* A frame structure pointer, named hand, for pointing a frame to evict. */
struct list_elem *hand;

/* filesys/file.c */
/* Closes FILE. */
void
file_close (struct file *file) 
{
  if (file != NULL)
    {
      file_allow_write (file);
      inode_close (file->inode);
      free (file); 
    }
}
/* Writes SIZE bytes from BUFFER into FILE,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs) 
{
  return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (file->deny_write) 
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}

/* Makes CNT sectors starting at SECTOR available for use. */
void free_map_release (disk_sector_t sector, size_t cnt)
{
  ASSERT (bitmap_all (free_map, sector, cnt));
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
}

/* filesys/free-map.h */
/* filesys/inode.c */
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / DISK_SECTOR_SIZE;
  else
    return -1;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}
/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
/* filesys/inode.h */
/* Returns the number of elements required for BIT_CNT bits. */
inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}
/* Returns the number of bytes required for BIT_CNT bits. */
inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}
/* Writes B to FILE.  Return true if successful, false
   otherwise. */
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
/* lib/kernel/bitmap.h */
/* userprog/pagedir.c */
/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  uint32_t *pt, *pde;

  ASSERT (pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
  ASSERT (!create || is_user_vaddr (vaddr));

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}

/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void * pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}

/* Returns true if the PTE for virtual page VPAGE in PD is dirty,
   that is, if the page has been modified since the PTE was
   installed.
   Returns false if PD contains no PTE for VPAGE. */
bool pagedir_is_dirty (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_D) != 0;
}

/* userprog/syscall.c */
#define fdtable thread_current ()->files
inline struct file* get_file(int fd) {
  if ((fd < 0) || (fd >= NUM_FD)) exit(-1);
  return fdtable[fd];
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

/* userprog/syscall.h */
/* vm/frame.c */
/* Initialize the frame table. */
void
ft_init (void) 
{
  list_init (&frame_list);
  lock_init (&frame_lock);
  
  hand = &frame_list.head;
}
/* vm/frame.h */
void ft_init (void);
/* vm/page.c */

static unsigned
page_hash (const struct hash_elem *element, void *aux UNUSED) {
  struct special_page_elem *page = hash_entry (element, struct special_page_elem, elem);
  return hash_bytes (&page->virtual_page, sizeof (page->virtual_page));
}

static bool
page_key_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct special_page_elem *pa = hash_entry (a, struct special_page_elem, elem);
  struct special_page_elem *pb = hash_entry (b, struct special_page_elem, elem);
  return pa->virtual_page < pb->virtual_page;
}

void
init_supplemental_pagetable (struct thread *t) {
  hash_init (&t->sup_pagetable, page_hash, page_key_less, NULL);
  sema_init (&t->page_sema, 1);
}
void
expire_page (struct special_page_elem * gen_page) {
#ifdef USERPROG
  struct thread *cur = thread_current ();
  void *kpage = pagedir_get_page (cur->pagedir, (void *)gen_page->virtual_page);
  if (kpage != NULL){
    switch(gen_page->type) {
    case FILE:
      noop ();
      struct file_page *file_page = (struct file_page *)gen_page;
      if (pagedir_is_dirty (cur->pagedir, (void *)file_page->virtual_page)) {
        lock_acquire (&filesys_lock);
        file_write_at (file_page->source_file, (void *)file_page->virtual_page, 
                       file_page->zero_after, file_page->offset);
        lock_release (&filesys_lock);
      }
      break;
    default:
      break;
    }
  }
  hash_delete (&cur->sup_pagetable, &gen_page->elem);
  free(gen_page);
#endif
}

static void expire_page_hf (struct hash_elem *element, void *aux UNUSED) {
  expire_page (hash_entry (element, struct special_page_elem, elem));
}

void
destroy_supplemental_pagetable (struct thread *t) {
  sema_down (&t->page_sema);
  hash_destroy (&t->sup_pagetable, expire_page_hf);
  sema_up (&t->page_sema);
}
/* vm/swap.c */
bool swap_slot_less(struct list_elem *a, struct list_elem *b, void *aux);
/* Initialize the swap table. */
void
swap_init (void) 
{
  list_init (&free_swap_list);
  lock_init (&swap_lock);
  
  swap_disk = disk_get (1, 1);
  
  struct free_swap_slot *free_ss = malloc (sizeof (struct free_swap_slot));
  free_ss->start = 0;
  free_ss->size = disk_size(swap_disk);

  lock_acquire (&swap_lock);
  list_insert_ordered(&free_swap_list, &free_ss->free_swap_elem, (list_less_func *)swap_slot_less, NULL);
  lock_release (&swap_lock);
}
/* swap slot comparison function */
bool swap_slot_less(struct list_elem *a, struct list_elem *b, void *aux UNUSED)
{
    bool rv = false;
    struct free_swap_slot *fss_a, *fss_b;
    fss_a = list_entry(a, struct free_swap_slot, free_swap_elem);
    fss_b = list_entry(b, struct free_swap_slot, free_swap_elem);    
    
    if (fss_a->start < fss_b->start) 
    {
        rv = true;
    }
    return rv;
}
