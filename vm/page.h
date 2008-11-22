#ifndef VM_PAGE_H_
#define VM_PAGE_H_
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum special_page {
  EXEC,
  FILE,
  SWAP,
  ZERO
  };

//this just defines the head of the struct which all special
// page elements must begin with
struct special_page_elem {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
};


struct exec_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
  struct file *elf_file;
  size_t offset; //number of bytes into the file to start
  size_t zero_after; //after this offset, the rest should be zeros
  bool writable;
};

struct file_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
  struct file source_file;
  size_t offset; //number of pages into the file
};

struct swap_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
  size_t offset; //number of pages into the swap
};

struct zero_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
};

void init_supplemental_pagetable (struct hash *sup_pagetable);
void add_lazy_page (struct special_page_elem *page);
struct special_page_elem * find_lazy_page (uint32_t ptr);

#endif /*VM_PAGE_H_*/
