#include <debug.h>
#include "threads/thread.h"
#include "page.h"
#include "filesys/file.h"
#include "filesys/inode.h"
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
add_lazy_page (struct special_page_elem *page) {
  struct hash_elem *elem = hash_insert (&thread_current ()->sup_pagetable, &page->elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

struct special_page_elem *
find_lazy_page (uint32_t ptr) {
  struct special_page_elem needle;
  needle.virtual_page = 0xfffff000 & ptr;
  struct hash_elem *elem = hash_find (&thread_current ()->sup_pagetable, &needle.elem);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}

static void noop(void);
static inline void noop() {} 
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
    printf(" from file ");
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
  }
  printf("\n");
}

void
print_supplemental_page_table () {
  hash_apply (&thread_current ()->sup_pagetable, print_page_entry);
}

