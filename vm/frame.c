#include <debug.h>
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

/* List of used frame. */
static struct list frame_list;
static struct lock frame_lock;

/* Initialize the frame table. */
void
ft_init (void) 
{
  lock_init (&frame_lock);
  list_init (&frame_list);
}

/* Get a user page from user pool, and add to our frame table if successful. */
void *
ft_get_page (enum palloc_flags flags)
{
	ASSERT (flags & PAL_USER);

	void *page = palloc_get_page (flags);
	
	if (page != NULL)
	{
		struct frame *f = malloc (sizeof (struct frame));
		f->tid = thread_current()->tid;
		f->user_page = page;
	
		lock_acquire (&frame_lock);
		list_push_back(&frame_list, &f->ft_elem);
		lock_release (&frame_lock);
	}
	
	return page;
}

/* Destroy all the page references of thread t in frame table. */
void
ft_destroy (struct thread *t)
{
	lock_acquire (&frame_lock);

	struct list_elem *elem = list_begin(&frame_list);

	while(elem != list_end(&frame_list)) 
	{
		struct frame *f = list_entry(elem, struct frame, ft_elem);
		
		if (f->tid == t->tid)
		{
			list_remove(elem);
			elem = list_next(elem);
			free(f);
		}
		else
			elem = list_next(elem);
	}
	
	lock_release (&frame_lock);
}
