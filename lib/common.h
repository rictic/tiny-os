#ifndef _COMMON_H
#define _COMMON_H


/* filesys/filesys.c */
struct disk *filesys_disk;
/* filesys/free-map.c */
struct file *free_map_file;   /* Free map file. */
struct bitmap *free_map;      /* Free map, one bit per disk sector. */

#endif
