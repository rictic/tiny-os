#ifndef VM_SWAP_H_
#define VM_SWAP_H_

#include <list.h>
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/palloc.h"

/* The number of sectors in each frame. */
#define SECTORS_PER_FRAME 8

/* frame structure for frame table. */
struct swap_slot
{
    disk_sector_t start;                /* First data sector of the swap slot. */
};

struct free_swap_slot
{
    disk_sector_t start;                /* First data sector of the free swap slot. */
    size_t size;						/* Size of this free slot in sectors. */
    struct list_elem free_swap_elem;    /* List free swap element. */
};

void swap_init (void);
bool swap_slot_read (void *frame, struct swap_slot* ss);
struct swap_slot* swap_slot_write (void *frame);

#endif /*VM_SWAP_H_*/
