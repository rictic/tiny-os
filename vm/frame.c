#include <debug.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

/* List of used frame. */
//static struct list frame_list;

/* A frame structure pointer, named hand, for pointing a frame to evict. */
static struct list_elem *hand;

static void *ft_replacement (void);
static inline void check_and_set_hand (void);

/* Initialize the frame table. */
void
ft_init (void) 
{
  list_init (&frame_list);
  lock_init (&frame_lock);
  
  hand = &frame_list.head;
}

/* Get a user page from user pool, and add to our frame table if successful. */
struct frame *
ft_get_page (enum palloc_flags flags)
{
  ASSERT (flags & PAL_USER);

  lock_acquire (&frame_lock);

  void *page = palloc_get_page (flags);
  struct frame *f = NULL;
  
  if (page == NULL)
  {
    page = ft_replacement();

    if (page == NULL)
      return NULL;

    //f->t = thread_current();
    //f->tid = thread_current()->tid;
  }
  
  f = malloc (sizeof (struct frame));
  f->t = thread_current();
  //f->tid = thread_current()->tid;
  f->user_page = page;
  f->loaded = false;
  //*f->PTE &= ~(uint32_t) PTE_A; 
  
  list_push_back(&frame_list, &f->ft_elem);
  lock_release (&frame_lock);
  
  return f;
}

/* Free an allocated page and also remove the page reference in the frame table. */
void
ft_free_page (void *page)
{ 
  struct list_elem *elem;
  struct frame *f;
  
  lock_acquire (&frame_lock);

  //printf("ft_free_page!\n");

  lforeach(elem, &frame_list)
  {
    f = list_entry(elem, struct frame, ft_elem);

    if (f->user_page == page && f->t == thread_current())
    {
      if (hand == &f->ft_elem)
      {
    	  hand = list_next(hand);
    	  check_and_set_hand();
      }	  
      list_remove(elem);
      free(f);
      break;
    }
  }
  
  lock_release (&frame_lock);
  
  palloc_free_page (page);
}

/* Destroy all the page references of thread t in frame table. */
void
ft_destroy (struct thread *t)
{
  //lock_acquire (&frame_lock);

  struct list_elem *elem = list_begin(&frame_list);

  //printf("ft_destroy!\n");
  while(elem != list_end(&frame_list)) 
  {
    struct frame *f = list_entry(elem, struct frame, ft_elem);
    
    if (f->t == t)
    {
      if (hand == &f->ft_elem)
      {
    	  hand = list_next(hand);
    	  check_and_set_hand();
      }	  
      list_remove(elem);
      elem = list_next(elem);
      free(f);
    }
    else
      elem = list_next(elem);
  }
  
  //lock_release (&frame_lock);
}

static struct frame *
get_frame_for_replacement(void) {
  check_and_set_hand();
  struct frame *f = list_entry(hand, struct frame, ft_elem);
  
  /* Second Chance replacement algorithm. */
  /* Choose the next page with Access bit not set. */
  while ((f->loaded == true && (*f->PTE & PTE_A) != 0) || f->loaded == false)
  {
	if (f->loaded != false)
	{	
		//*f->PTE &= ~(uint32_t) PTE_A;
		pagedir_set_accessed (f->t->pagedir, f->virtual_address, false);
		//hand = list_remove(hand);
		//list_push_back(&frame_list, &f->ft_elem);
	}
	
	hand = list_next(hand);
    
    check_and_set_hand();
    //printf("%08x\n", hand);
    f = list_entry(hand, struct frame, ft_elem);
  }
  return f;
}

static void *
ft_replacement (void)
{
  ASSERT (!list_empty(&frame_list));
  //lock_acquire (&frame_lock);
  enum intr_level old_level = intr_disable ();
  struct frame *f = get_frame_for_replacement ();
  void * kpage = f->user_page;
  bool dirty = (*f->PTE) & PTE_D;
  void * user_page = f->virtual_address;
  struct thread *evict_t = f->t;
  struct special_page_elem *evicted_page = find_lazy_page(f->t, (uint32_t)f->virtual_address);
  pagedir_clear_page(f->t->pagedir, f->virtual_address);
  
  sema_down(&evict_t->page_sema);
  intr_set_level (old_level);
    
  if (dirty) {
    if (evicted_page != NULL && evicted_page->type == FILE){
      struct file_page *file_page = (struct file_page*) evicted_page;
      file_write_at (file_page->source_file, kpage, file_page->zero_after, file_page->offset);      
    }
    else {
      struct swap_slot *ss = swap_slot_write(kpage);
      //ASSERT(ss != NULL && !!"unable to obtain a swap slot");
      if (ss == NULL)
      {	  
    	  sema_up (&evict_t->page_sema);
    	  lock_release (&frame_lock);
    	  return NULL;
      }
      
      if (evicted_page != NULL)
        hash_delete(&evict_t->sup_pagetable, &evicted_page->elem);
      add_lazy_page_unsafe (evict_t, (struct special_page_elem*)
                     new_swap_page (user_page, ss, dirty, evicted_page));
    }
  }

  if (hand == &f->ft_elem)
  {
	  hand = list_next(hand);
	  check_and_set_hand();
  }	  

  list_remove(&f->ft_elem);
  free(f);
  
  sema_up (&evict_t->page_sema);
  
//   lock_acquire (&frame_lock);
  hand = list_next(hand);
  check_and_set_hand();

  /*f->t = NULL;
  f->PTE = NULL;
  f->virtual_address = NULL;
  f->loaded = false;*/

  //lock_release (&frame_lock);

  return kpage;
}

static inline void 
check_and_set_hand (void)
{
  if (hand == &frame_list.head)
    hand = list_next(hand);
  
  if (hand == list_end(&frame_list))
    hand = list_begin(&frame_list);
}
