#ifndef FILESYS_FREE_MAP_H
#define FILESYS_FREE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"

struct bitmap *free_map;      /* Free map, one bit per disk sector. */
struct file *free_map_file;   /* Free map file. */

void free_map_init (void);
void free_map_read (void);
void free_map_create (void);
void free_map_open (void);
void free_map_close (void);

bool free_map_allocate (size_t, disk_sector_t *);
void free_map_release (disk_sector_t, size_t);


#endif /* filesys/free-map.h */
