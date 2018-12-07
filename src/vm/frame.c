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

/* Creates a new empty struct frame for vaddr,
 and ads it to the frame_table list */
void create_frame(void *kvaddr)
{
  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->kvaddr = kvaddr;
  list_init(&new_frame->user_ptes);
  new_frame->pin = false;
  lock_init(&new_frame->lock);
  list_push_back(&frame_table, &new_frame->elem);
}

/* Finds the struct frame for kvaddr in the frame_table list,
 and sets the other fields according to uaddr and current page directory */
void set_frame(void *kvaddr, void *uaddr)
{
  struct user_pte_ptr *new_pte_ptr = malloc(sizeof(struct user_pte_ptr));
  new_pte_ptr->pagedir = thread_current()->pagedir;
  new_pte_ptr->uaddr = uaddr;

  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e))
  {
    struct frame *curr = list_entry(e, struct frame, elem);
    lock_acquire(&curr->lock);
    if (curr->kvaddr == kvaddr)
    {
      list_push_back(&curr->user_ptes, &new_pte_ptr->elem);
      curr->pin = false;
      lock_release(&curr->lock);
      return;
    }
    lock_release(&curr->lock);
  }
  /* We always call set_frame after create_frame, so kvaddr should always 
  be present in the frame table. This means that the function should return
  somewhere in the for loop, and this line should never be reached */
  free(new_pte_ptr);
  NOT_REACHED();
}


/* Uses the "clock" algorithm. Considers frames for eviction in a round robin 
   way (cyclically) and if a page in the frame's user_ptes list was accessed 
   since the last consideration then its frame is not replaced. Uses the accessed
   bit of pds to store whether it was accessed and checks the frame's user_ptes.
   If the frame selected has its pin set to true we need to choose a different one */
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

  return frame_to_evict->kvaddr;
}
