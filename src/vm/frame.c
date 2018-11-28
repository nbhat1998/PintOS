#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

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
  struct list_elem *e = list_begin(&frame_table);

  while (e != list_end(&frame_table))
  {
    struct frame *curr = list_entry(e, struct frame, elem);

    if (curr->physical_memory_key == physical_memory_key)
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

  while (e != list_end(&frame_table))
  {
    struct frame *curr = list_entry(e, struct frame, elem);

    //if (curr->uaddr == physical_memory_key)

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

    e = list_next(e);
  }
}