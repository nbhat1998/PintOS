#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>
#include "threads/synch.h"

struct frame             /* Struct keeping track of a frame and the */
{                        /* Ptes pointing to in */
  void *kvaddr;          /* Pointer to page in kernel virtual memory that has a 
                            1:1 link to physical memory */
  struct list user_ptes; /* List that stores user_pte_ptr structs. used to keep 
                            track of pts pointing to the frame */
  struct list_elem elem; /* Elem to store struct in frame_table list */
  bool pin;              /* Pin used for eviction syncronization between 2 threads */
  struct lock lock;      /* Lock used for fine grained locking */
};

struct user_pte_ptr /* Struct used to get a pte */
{
  void *pagedir;         /* Page directory the pte is in */
  void *uaddr;           /* User virtual address used to index in the pagedir */
  struct list_elem elem; /* Elem used to add in frame user_ptes list */
};

struct list frame_table; /* List of frames used as a frame table */

void create_frame(void *vaddr);
void set_frame(void *kvaddr, void *uaddr);
void remove_frames(void *kvaddr);

struct list_elem *evict_ptr; /* Used in eviction algorithm for second chance */

void *evict();
#endif /* vm/frame.h */
