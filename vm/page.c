#include <debug.h>
#include "threads/thread.h"
#include "page.h"

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

void
add_lazy_page (struct special_page_elem *page) {
  hash_insert (&thread_current ()->sup_pagetable, &page->elem);
}

struct special_page_elem *
find_lazy_page (uint32_t ptr) {
  struct special_page_elem needle;
  needle.virtual_page = 0xfffff000 & ptr;
  struct hash_elem *elem = hash_find (&thread_current ()->sup_pagetable, &needle);
  if (elem == NULL) return NULL;
  return hash_entry(elem, struct special_page_elem, elem);
}
