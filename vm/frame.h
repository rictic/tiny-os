#ifndef VM_FRAME_H_
#define VM_FRAME_H_

#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

/* Lock for the frame table. */
struct lock frame_lock;

/* List of used frame. */
struct list frame_list;

/* frame structure for frame table. */
struct frame {
  //tid_t tid;                          /* Thread identifier. */
  struct thread *t;					/* The thread the frame belongs to. */
  uint32_t *user_page;				/* the pointer to the used user frame. */
  uint32_t *PTE;						/* the page table entry for the user page. */
  uint32_t *virtual_address;			/* the user virtual address for this frame. */
  struct list_elem ft_elem;         	/* List frame element. */
  bool loaded;
};

struct frame *ft_get_page (enum palloc_flags);
void ft_free_page (void *);
void ft_destroy (struct thread *);
void ft_init (void);

#endif /*VM_FRAME_H_*/
