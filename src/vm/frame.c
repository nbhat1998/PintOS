#include "vm/frame.h"
#include <string.h>
#include <random.h>
#include <list.h>
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/swap.h"

// TODO: set pin to false in install_page

void create_frame(void *vaddr)
{
  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->vaddr = vaddr;
  list_init(&new_frame->user_ptes);
  // TODO: set pint to true
  list_push_back(&frame_table, &new_frame->elem);
}

void set_frame(void *vaddr, void *uaddr)
{
  struct user_pte_ptr *new_pte_ptr = malloc(sizeof(struct user_pte_ptr));
  new_pte_ptr->pagedir = thread_current()->pagedir;
  new_pte_ptr->uaddr = uaddr;

  for (struct list_elem *e = list_begin(&frame_table);
       e != list_end(&frame_table); e = list_next(e))
  {
    struct frame *curr = list_entry(e, struct frame, elem);
    if (curr->vaddr == vaddr)
    {
      list_push_back(&curr->user_ptes, &new_pte_ptr->elem);
      return;
    }
  }
  NOT_REACHED();
}

void remove_frames(uint32_t *vaddr)
{
  struct list_elem *e = list_begin(&frame_table);

  while (e != list_end(&frame_table))
  {
    struct frame *curr = list_entry(e, struct frame, elem);

    if (curr->vaddr == vaddr)
    {
      struct list_elem *f = list_begin(&curr->user_ptes);

      while (f != list_end(&curr->user_ptes))
      {
        struct user_pte_ptr *current = list_entry(f, struct user_pte_ptr, elem);
        uint32_t *pte = get_pte(current->pagedir, current->uaddr, false);
        if (pte != NULL)
        {
          /* Set the present bit (bit 0) to 0 */
          pte = (uint32_t)pte & ((1 << 32) - 2);
        }

        struct list_elem *temp = f;
        f = list_next(f);
        list_remove(temp);
        free(current);
      }

      list_remove(e);
      free(curr);
      return;
    }

    e = list_next(e);
  }
}

void remove_uaddr(uint32_t *uaddr)
{
  struct list_elem *e = list_begin(&frame_table);
  // TODO: set pin to true
  while (e != list_end(&frame_table))
  {
    struct frame *curr = list_entry(e, struct frame, elem);
    struct list_elem *f = list_begin(&curr->user_ptes);

    while (f != list_end(&curr->user_ptes))
    {
      struct user_pte_ptr *current = list_entry(f, struct user_pte_ptr, elem);

      if (current->uaddr == uaddr)
      {
        struct list_elem *temp = f;
        f = list_next(f);
        list_remove(temp);
        free(current);
      }
      else
      {
        f = list_next(f);
      }
    }

    if (list_empty(&curr->user_ptes))
    {
      // TODO : free frame and page in memory if no thread's uaddr is pointing to it
    }

    e = list_next(e);
  }
}

void *evict()
{
  int index_to_evict = random_ulong() % list_size(&frame_table);
  int index = 0;
  struct list_elem *curr = list_begin(&frame_table);
  while (index < index_to_evict)
  {
    curr = list_next(&curr);
    index++;
  }
  struct frame *frame_to_evict = list_entry(curr, struct frame, elem);
  swap_write(frame_to_evict);
  memset(frame_to_evict->vaddr, 0, PGSIZE);
  return frame_to_evict->vaddr;
}
