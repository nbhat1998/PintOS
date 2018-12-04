#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>

#define MAX_FRAMES 1 << 16

struct frame
{
  uint32_t *vaddr;
  // TODO: maybe have a flag if it's a file and what file it is
  struct list user_ptes;
  struct list_elem elem;
  bool pin;
};

struct user_pte_ptr
{
  uint32_t *pagedir;
  uint32_t *uaddr;
  struct list_elem elem;
};

struct list frame_table;

void create_frame(void *vaddr);
void set_frame(void *vaddr, void *uaddr);
void remove_frames(uint32_t *vaddr);

void *evict();
#endif /* vm/frame.h */
