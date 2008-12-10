#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/disk.h"

/* List of free swap slots. */
static struct list free_swap_list;

/* Lock for the swap table. */
static struct lock swap_lock;

/* The disk that contains the file system. */
static struct disk *swap_disk;

static disk_sector_t alloc_swap_slot (void);
static void free_swap_slot (struct swap_slot *ss);
static bool swap_slot_less(struct list_elem *a, struct list_elem *b, void *aux);

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

/* Read the swap slot into the frame and return true if successful. */
bool
swap_slot_read (void *frame, struct swap_slot* ss)
{
	off_t pos = 0;

	if (frame == NULL)
    return false;

	disk_sector_t start = ss->start;
	
	/* Read from the swap disk into frame. */
	for (pos = 0; pos < PGSIZE; pos += DISK_SECTOR_SIZE, start++)
		disk_read(swap_disk, start, frame + pos);

	free_swap_slot(ss);
	
	return true;
}

/* Write one frame of thread t to the swap disk. Return that swap slot if successful. 
   Otherwise, return NULL. */
struct swap_slot*
swap_slot_write (void *frame)
{
	off_t pos;
	struct swap_slot *ss = NULL;
	
	if (frame != NULL)
	{
		disk_sector_t start = alloc_swap_slot();
		
		if (start != disk_size(swap_disk)+1)
		{
			ss = malloc (sizeof (struct swap_slot));
			//ss->tid = t->tid;
			ss->start = start;
			
			/* Write to the swap disk from frame. */
			for (pos = 0; pos < PGSIZE; pos += DISK_SECTOR_SIZE, start++)
				disk_write(swap_disk, start, frame + pos);	
		}	
	}
	
	return ss;
}

/* Allocate a swap slot from swap disk. */
static disk_sector_t
alloc_swap_slot (void)
{
	disk_sector_t start = disk_size(swap_disk)+1; // Define as the false sector.
	
	lock_acquire (&swap_lock);
	
	if (!list_empty (&free_swap_list))
	{
		struct free_swap_slot *free_ss;
	    struct list_elem *elem;

		lforeach(elem, &free_swap_list)
		{
			free_ss = list_entry (elem, struct free_swap_slot, free_swap_elem);
			if (free_ss->size >= SECTORS_PER_FRAME)
				break;
		}
		
		if (elem != list_end(&free_swap_list))
		{
			start = free_ss->start;
			
			if (free_ss->size == SECTORS_PER_FRAME)
			{
				list_remove(&free_ss->free_swap_elem);
				free(free_ss);
			}	
			else
			{
				free_ss->start += SECTORS_PER_FRAME;
				free_ss->size -= SECTORS_PER_FRAME;
			}			
		}
	}	

	lock_release (&swap_lock);
	
	return start;
}

/* Free a swap slot to swap disk. */
static void
free_swap_slot (struct swap_slot *ss)
{
	  struct free_swap_slot *free_ss = malloc (sizeof (struct free_swap_slot));
	  free_ss->start = ss->start;
	  free_ss->size = SECTORS_PER_FRAME;

	  lock_acquire (&swap_lock);
	  
	  list_insert_ordered(&free_swap_list, &free_ss->free_swap_elem, (list_less_func *)swap_slot_less, NULL);
	  
	  struct free_swap_slot *temp_free_ss;
	  struct list_elem *temp_elem = list_next(&free_ss->free_swap_elem);
	  
	  /* Try to merge with the swap slot on the right. */
	  if (temp_elem != list_end(&free_swap_list))
	  {
		  temp_free_ss = list_entry (temp_elem, struct free_swap_slot, free_swap_elem);	
		  if (free_ss->start + free_ss->size == temp_free_ss->start)
		  {
			  free_ss->size += temp_free_ss->size;
			  list_remove(temp_elem);
			  free (temp_free_ss);
		  }	  
	  }	  
	  	  
	  /* Try to merge with the swap slot on the left. */
	  if (&free_ss->free_swap_elem != list_begin(&free_swap_list))
	  {
		  temp_free_ss = list_entry (list_prev(&free_ss->free_swap_elem), struct free_swap_slot, free_swap_elem);	
		  if (free_ss->start == temp_free_ss->start + temp_free_ss->size)
		  {
			  temp_free_ss->size += free_ss->size;
			  list_remove(&free_ss->free_swap_elem);
			  free (free_ss);
		  }	  
	  }	  
	  
	  lock_release (&swap_lock);
	  free(ss);
}

/* swap slot comparison function */
static bool swap_slot_less(struct list_elem *a, struct list_elem *b, void *aux UNUSED)
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
