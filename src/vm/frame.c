#include "vm/frame.h"
#include "vm/swap.h"
#include <string.h>
#include <random.h>
#include <list.h>
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/pte.h"

// TODO: set pin to false in install_page

void create_frame(void *vaddr)
{
  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->kaddr = vaddr;
  list_init(&new_frame->user_ptes);
  // TODO: set pint to true
  new_frame->pin = false;
  lock_init(&new_frame->lock);
  list_push_back(&frame_table, &new_frame->elem);
}

void set_frame(void *kaddr, void *uaddr)
{
  struct user_pte_ptr *new_pte_ptr = malloc(sizeof(struct user_pte_ptr));
  new_pte_ptr->pagedir = thread_current()->pagedir;
  new_pte_ptr->uaddr = uaddr;

  //printf("pd %p uaddr %p pte: %p\n", thread_current()->pagedir, uaddr, get_pte(thread_current()->pagedir, uaddr, false));
  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e))
  {
    struct frame *curr = list_entry(e, struct frame, elem);
    lock_acquire(&curr->lock);
    if (curr->kaddr == kaddr)
    {
      list_push_back(&curr->user_ptes, &new_pte_ptr->elem);
      curr->pin = false;
      lock_release(&curr->lock);
      return;
    }
    lock_release(&curr->lock);
  }
  free(new_pte_ptr);
  NOT_REACHED();
}

void *evict()
{
  struct frame *frame_to_evict;
  do
  {
    int index_to_evict = evict_cnt % list_size(&frame_table);
    int index = 0;

    for (struct list_elem *e = list_begin(&frame_table);
         e != list_end(&frame_table); e = list_next(e))
    {
      frame_to_evict = list_entry(e, struct frame, elem);
      if (index == index_to_evict)
      {
        break;
      }
      index++;
    }
    evict_cnt++;
  } while (frame_to_evict->pin);

  frame_to_evict->pin = true;
  // TODO: check if dirty, and only write to swap if true. Also pinning.

  swap_write(frame_to_evict);

  return frame_to_evict->kaddr;
}
