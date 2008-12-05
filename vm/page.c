#include <debug.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "page.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include <stdio.h>

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
init_supplemental_pagetable (struct hash *sup_pagetable) {
  hash_init (sup_pagetable, page_hash, page_key_less, NULL);
}

struct special_page_elem *
add_lazy_page (struct thread *t, struct special_page_elem *page) {
  struct hash_elem *elem = hash_insert (&t->sup_pagetable, &page->elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

struct zero_page *
new_zero_page (uint32_t virtual_page) {
  struct zero_page *zp = malloc (sizeof (struct zero_page));
  zp->type = ZERO; zp->virtual_page = virtual_page;
  return zp;
}

struct exec_page *
new_exec_page (uint32_t virtual_page, struct file *elf_file, 
               size_t offset, size_t zero_after, bool writable) {
  struct exec_page *ep = malloc (sizeof (struct exec_page));
  ep->type = EXEC; ep->virtual_page = virtual_page; ep->elf_file = elf_file;
  ep->offset = offset; ep->zero_after = zero_after; ep->writable = writable;
  return ep;
}

struct special_page_elem *
find_lazy_page (struct thread *t, uint32_t ptr) {
  struct special_page_elem needle;
  needle.virtual_page = 0xfffff000 & ptr;
  struct hash_elem *elem = hash_find (&t->sup_pagetable, &needle.elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

static inline void print_file(struct file *file) {
  printf ("file at sector ");
  print_inode_location (file->inode);
}
static void
print_page_entry (struct hash_elem *e, void *aux UNUSED) {
  struct special_page_elem *gen_page = hash_entry(e, struct special_page_elem, elem);
  printf("%s page mapped to 0x%08x", special_page_name(gen_page->type), gen_page->virtual_page);
  switch (gen_page->type) {
  case EXEC:
    noop();
    struct exec_page *exec_page = (struct exec_page*) gen_page;
    printf(" from ");
    print_file(exec_page->elf_file);
    printf(" starting at offset %u, but zeroing after %u", (unsigned)exec_page->offset, (unsigned)exec_page->zero_after);
  case FILE:
    noop();
//     struct file_page *file_page = (struct file_page*) gen_page;
    break;
  case SWAP:
    noop();
//     struct swap_page *swap_page = (struct swap_page*) gen_page;
    break;
  case ZERO:
    break;
  case STACK:
    break;
  }
  printf("\n");
}

void
print_supplemental_page_table () {
  hash_apply (&thread_current ()->sup_pagetable, print_page_entry);
}

void
expire_page (struct special_page_elem * gen_page) {
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
//     ft_free_page(kpage);
  }
  hash_delete (&cur->sup_pagetable, &gen_page->elem);
  free(gen_page);
}

static void expire_page_hf (struct hash_elem *element, void *aux UNUSED) {
  expire_page (hash_entry (element, struct special_page_elem, elem));
}

void
destroy_supplemental_pagetable (struct hash *sup_pagetable) {
  hash_destroy (sup_pagetable, expire_page_hf);
}

bool
validate_free_page (void *upage, uint32_t read_bytes)
{
	/* Failed if the range of pages mapped overlaps any existing set of mapping pages. 
	   (Stack validation not implimented yet!) */
	size_t num_of_pages = read_bytes / PGSIZE;
	if (read_bytes % PGSIZE != 0)
		num_of_pages++;
	unsigned i;
	uint32_t ptr = (uint32_t)upage;
	struct special_page_elem *spe;
	for(i = 0; i < num_of_pages; i++, ptr += PGSIZE)
	{
		spe = find_lazy_page(thread_current (), ptr);
		if (spe != NULL)
			return false; // This page has been already mapped.
	}
	
	return true;
}
