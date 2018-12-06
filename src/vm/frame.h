#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>
#include "threads/synch.h"

struct frame
{
  uint32_t *kaddr;
  struct list user_ptes;
  struct list_elem elem;
  bool pin;
  struct lock lock;
};

struct user_pte_ptr
{
  uint32_t *pagedir;
  uint32_t *uaddr;
  struct list_elem elem;
};

struct list_elem *evict_ptr;

struct list frame_table;

void create_frame(void *vaddr);
void set_frame(void *vaddr, void *uaddr);
void remove_frames(uint32_t *vaddr);

void *evict();
#endif /* vm/frame.h */
