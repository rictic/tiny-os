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

//static inline struct special_page_elem *add_lazy_page_unsafe (struct thread *t, struct special_page_elem *page);

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

struct special_page_elem *
add_lazy_page_unsafe (struct thread *t, struct special_page_elem *page) {
  ASSERT(((unsigned)page & 0xfffff000) != 0xccccc000);
  struct hash_elem *elem = hash_insert (&t->sup_pagetable, &page->elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

struct special_page_elem *
add_lazy_page (struct thread *t, struct special_page_elem *page) {
  ASSERT(page != 0xdddddddd);
  sema_down (&t->page_sema);
  struct special_page_elem *results = add_lazy_page_unsafe(t, page);
  sema_up (&t->page_sema);
  return results;
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

struct swap_page *
new_swap_page (uint32_t virtual_page, struct swap_slot *slot, 
               bool dirty, struct special_page_elem *evicted_page){
  struct swap_page *sp = malloc (sizeof (struct swap_page));
  sp->type = SWAP; sp->virtual_page = virtual_page; sp->slot = slot;
  sp->dirty = dirty; sp->evicted_page = evicted_page;
  return sp;
}

static inline struct special_page_elem *find_lazy_page_unsafe (struct thread *t, uint32_t ptr) {
  ASSERT((ptr & 0xfffff000) != 0xccccc000);
  struct special_page_elem needle;
  needle.virtual_page = 0xfffff000 & ptr;
  struct hash_elem *elem = hash_find (&t->sup_pagetable, &needle.elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

struct special_page_elem *
find_lazy_page (struct thread *t, uint32_t ptr) {
  sema_down (&t->page_sema);
  struct special_page_elem *result = find_lazy_page_unsafe (t, ptr);
  sema_up (&t->page_sema);
  return result;
}

static inline void print_file(struct file *file) {
  printf ("file at sector ");
  print_inode_location (file->inode);
}

static void
print_page_entry_hf (struct hash_elem *e, void *aux UNUSED) {
  print_page_entry(hash_entry(e, struct special_page_elem, elem));
}

void
print_page (unsigned char * ptr) {
  unsigned i, j, k;
  for (k = 0; k < (PGSIZE / 16); k++) {
    printf("%08x  ", ptr);
    for(j=0; j<2;j++){
        for(i=8*j; (i<8+8*j) && (i<16); i++)
            printf("%02x ",ptr[i]);
        printf(" ");
    }
    printf("|");
    for(i=0; i<16; i++){
        if (ptr[i] >= 32 && ptr[i] <= 126)
            printf("%c",ptr[i]);
        else
            printf(".");
    }
    printf("|");
    printf("\n");
    ptr += 16;
  }
}

void
print_page_entry (struct special_page_elem *gen_page) {
  if (gen_page == NULL){
    printf("(NULL)\n");
    return;
  }
  printf("%s page mapped to 0x%08x", special_page_name(gen_page->type), gen_page->virtual_page);
  switch (gen_page->type) {
  case EXEC:
    noop();
    struct exec_page *exec_page = (struct exec_page*) gen_page;
    printf(" from ");
    print_file(exec_page->elf_file);
    printf(" starting at offset %u", (unsigned)exec_page->offset);
    if (exec_page->zero_after != 4096)
      printf(", but zeroing after %u", (unsigned)exec_page->zero_after);
    break;
  default:
    break;
  }
  printf("\n");
}

void
print_supplemental_page_table () {
  hash_apply (&thread_current ()->sup_pagetable, print_page_entry_hf);
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
  }
  hash_delete (&cur->sup_pagetable, &gen_page->elem);
  free(gen_page);
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
