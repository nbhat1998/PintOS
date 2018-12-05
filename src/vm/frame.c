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
  if (evict_ptr == NULL)
  {
    evict_ptr = list_begin(&frame_table);
  }
  struct frame *frame_to_evict;
  bool has_second_chance;
  do
  {
    frame_to_evict = list_entry(evict_ptr, struct frame, elem);
    lock_acquire(&frame_to_evict->lock);
    has_second_chance = false;
    for (struct list_elem *e = list_begin(&frame_to_evict->user_ptes);
         e != list_end(&frame_to_evict->user_ptes); e = list_next(e))
    {
      struct user_pte_ptr *user_page = list_entry(e, struct user_pte_ptr, elem);
      if (pagedir_is_accessed(user_page->pagedir, user_page->uaddr))
      {
        has_second_chance = true;
        pagedir_set_accessed(user_page->pagedir, user_page->uaddr, false);
      }
    }

    /* Go to the next frame */
    if (evict_ptr->next == list_end(&frame_table))
    {
      evict_ptr = list_begin(&frame_table);
    }
    else
    {
      evict_ptr = list_next(evict_ptr);
    }
    lock_release(&frame_to_evict->lock);
  } while (frame_to_evict->pin && has_second_chance);

  frame_to_evict->pin = true;

  swap_write(frame_to_evict);

  return frame_to_evict->kaddr;
}
