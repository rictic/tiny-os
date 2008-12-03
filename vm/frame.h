#ifndef VM_FRAME_H_
#define VM_FRAME_H_

#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

/* frame structure for frame table. */
struct frame
{
    tid_t tid;                          /* Thread identifier. */
    enum special_page type;				/* Identify the type of this page. */
	uint32_t *user_page;				/* the pointer to the used user frame. */
	uint32_t *PTE;						/* the page table entry for the user page. */
    struct list_elem ft_elem;         	/* List frame element. */
};

void ft_init (void);
struct frame *ft_get_page (enum palloc_flags flags, enum special_page type);
void ft_free_page (void *page);
void ft_destroy (struct thread *t);

#endif /*VM_FRAME_H_*/
