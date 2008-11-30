#ifndef VM_PAGE_H_
#define VM_PAGE_H_
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum special_page {
  EXEC,
  FILE,
  SWAP,
  ZERO,
  STACK
};
static const char *(special_page_names[]) = {"EXEC", "FILE", "SWAP", "ZERO", "STACK"};
inline static const char * special_page_name(const enum special_page page_num) {
  return special_page_names[page_num];
}

  
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
  struct file *source_file;
  size_t offset; //number of pages into the file
  size_t zero_after; //after this offset, the rest should be zeros
};

struct swap_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
  disk_sector_t sector; //sector where the disk starts
};

struct zero_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
};

struct stack_page {
  enum special_page type;
  struct hash_elem elem;
  uint32_t virtual_page;
};


void init_supplemental_pagetable (struct hash *sup_pagetable);
struct special_page_elem * add_lazy_page (struct special_page_elem *page);
struct special_page_elem * find_lazy_page (uint32_t ptr);
bool validate_free_page (void *upage, uint32_t read_bytes);

void print_supplemental_page_table (void);
#endif /*VM_PAGE_H_*/
