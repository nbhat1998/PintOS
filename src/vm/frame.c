#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void create_frame(uint32_t *physical_memory_key, uint32_t *uaddr)
{

  struct user_pte_ptr *new_pte_ptr = malloc(sizeof(struct user_pte_ptr));
  new_pte_ptr->pagedir = thread_current()->pagedir;
  new_pte_ptr->uaddr = uaddr;

  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e))
  {
    struct frame *curr = list_entry(e, struct frame, elem);
    if (curr->physical_memory_key == physical_memory_key)
    {
      list_push_back(&curr->user_ptes, &new_pte_ptr->elem);
      return;
    }
  }


  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->physical_memory_key = physical_memory_key;
  list_push_back(&new_frame->user_ptes, new_pte_ptr);

  list_push_back(&frame_table, &new_frame->elem);
}

void remove_frames(uint32_t *physical_memory_key)
{
  // TODO: iterate through the list, find every frame and
  // remove from pagedir entries and from frame_table list
  // then free
}