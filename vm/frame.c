#include <debug.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

/* List of used frame. */
static struct list frame_list;

/* Lock for the frame table. */
static struct lock frame_lock;

/* A frame structure pointer, named hand, for pointing a frame to evict. */
static struct list_elem *hand;

static struct frame *ft_replacement (void);
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

  void *page = palloc_get_page (flags);
  struct frame *f = NULL;
  
  if (page != NULL)
  {
    f = malloc (sizeof (struct frame));
    f->t = thread_current();
    //f->tid = thread_current()->tid;
    f->user_page = page;
  
    lock_acquire (&frame_lock);
    list_push_back(&frame_list, &f->ft_elem);
    lock_release (&frame_lock);
  }
  else
  {
    f = ft_replacement();

    if (f == NULL)
      return NULL;

    f->t = thread_current();
    //f->tid = thread_current()->tid;
  }
  
  return f;
}

/* Free an allocated page and also remove the page reference in the frame table. */
void
ft_free_page (void *page)
{ 
  struct list_elem *elem;
  struct frame *f;
  
  lock_acquire (&frame_lock);

  lforeach(elem, &frame_list)
  {
    f = list_entry(elem, struct frame, ft_elem);

    if (f->user_page == page)
    {
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
  lock_acquire (&frame_lock);

  struct list_elem *elem = list_begin(&frame_list);

  while(elem != list_end(&frame_list)) 
  {
    struct frame *f = list_entry(elem, struct frame, ft_elem);
    
    if (f->t == t)
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

static struct frame *
ft_replacement (void)
{
  struct thread *evict_t;
  
  lock_acquire (&frame_lock);
  ASSERT (!list_empty(&frame_list));

  check_and_set_hand();
  struct frame *f = list_entry(hand, struct frame, ft_elem);
  
  evict_t = f->t;
  uint32_t *pte = f->PTE;
  
  enum intr_level old_level = intr_disable ();
  
  /* Second Chance replacement algorithm. */
  /* Choose one page with Access bit not set. */
  while ((*pte & PTE_A) != 0)
  {
    pagedir_set_accessed (evict_t->pagedir, f->virtual_address, false);
    hand = list_remove(hand);
    list_push_back(&frame_list, &f->ft_elem);
    
    check_and_set_hand();
    f = list_entry(hand, struct frame, ft_elem);
    evict_t = f->t;
    pte = f->PTE;
  }
  
  if ((*pte & PTE_D) != 0) {
    switch(f->type){
      case (FILE):
        noop ();
        struct file_page *file_page = (struct file_page*) find_lazy_page(evict_t, (uint32_t)f->virtual_address);
        ASSERT (file_page != NULL);

        file_write_at (file_page->source_file, f->user_page, file_page->zero_after, file_page->offset);
        break;
      default:
        noop ();
        intr_set_level (old_level);
        struct swap_slot *ss = swap_slot_write(f->user_page);
        old_level = intr_disable ();

        struct swap_page *swap_page = malloc (sizeof (struct swap_page));

        if (ss == NULL)
        {
          intr_set_level (old_level);
          lock_release (&frame_lock);
          return NULL;
        } 

        swap_page->type = SWAP;
        swap_page->virtual_page = (uint32_t)f->virtual_address;
        swap_page->slot = ss;
        swap_page->dirty = *pte & PTE_D;
        swap_page->type_before = f->type;
      
        if (f->type == EXEC)
        {
          struct exec_page *exec_page = (struct exec_page*)find_lazy_page (evict_t, (uint32_t)f->virtual_address);
          
          //swap_page->exec = exec_page;
          hash_delete (&evict_t->sup_pagetable, &exec_page->elem);
          free(exec_page);
        }
        
        add_lazy_page (evict_t, (struct special_page_elem*)swap_page);
    }
  }

  /* Clear all the information for this frame. */
  pagedir_clear_page(evict_t->pagedir, f->virtual_address);

  f->t = NULL;
  f->type = 0;
  f->PTE = NULL;
  f->virtual_address = NULL;
  
  intr_set_level (old_level);
  hand = list_next(hand);
  check_and_set_hand();

  lock_release (&frame_lock);
  return f;
}

static inline void 
check_and_set_hand (void)
{
  if (hand == &frame_list.head)
    hand = list_next(hand);
  
  if (hand == list_end(&frame_list))
    hand = list_begin(&frame_list);
}
