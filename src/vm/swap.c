#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/share.h"
#include <bitmap.h>
#include <string.h>
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/exception.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

/* Helpers to read/write whole pages */
void swap_page_read(size_t index, void *kvaddr);
void swap_page_write(size_t index, struct frame *f);

/* When we have to read from swap, first we try to get a new frame, 
either by evicting one or creating a new one. Then, we find and lock 
the struct frame while we read the page from swap into the frame. 
At the end, set the swap bit of the PTE back to 0. */
void swap_read(void *fault_addr)
{
  void *pte = get_pte(thread_current()->pagedir, fault_addr, false);

  size_t swap_index = ((*(uint32_t *)pte) & PTE_ADDR) >> PGBITS;
  bool writable = ((*(uint32_t *)pte) & PTE_W) != 0;
  void *kvaddr = palloc_get_page(PAL_USER);
  if (kvaddr == NULL)
  {
    kvaddr = evict();
  }
  else
  {
    create_frame(kvaddr);
  }

  bool success = link_page(fault_addr, kvaddr, writable);
  set_frame(kvaddr, fault_addr);

  struct frame *frame;
  for (struct list_elem *curr = list_begin(&frame_table);
       curr != list_end(&frame_table); curr = list_next(curr))
  {
    frame = list_entry(curr, struct frame, elem);
    lock_acquire(&frame->lock);
    if (frame->kvaddr == kvaddr)
    {
      lock_release(&frame->lock);
      break;
    }
    lock_release(&frame->lock);
  }

  lock_acquire(&frame->lock);
  swap_page_read(swap_index, kvaddr);
  lock_release(&frame->lock);

  *(uint32_t *)pte &= (~PTE_S);
}

/*  This functions takes a frame and writes its frame to swap,
 unless it was a read-only executable. In that case, we just modify all the
 PTEs that point to it to have the information of where to read the page from. */
void swap_write(struct frame *f)
{
  lock_acquire(&f->lock);

  bool found = false;
  struct shared_exec *curr_exec;
  for (struct list_elem *e = list_begin(&shared_execs);
       e != list_end(&shared_execs);
       e = list_next(e))
  {
    curr_exec = list_entry(e, struct shared_exec, elem);
    if (curr_exec->kvaddr == f->kvaddr)
    {
      found = true;
      break;
    }
  }
  if (found)
  {
    const int PAGE_MID_FLAG = 0x400;
    const int FULL_PAGE_FLAG = 0x100;
    while (!list_empty(&f->user_ptes))
    {
      struct list_elem *curr_elem = list_pop_front(&f->user_ptes);
      struct user_pte_ptr *current = list_entry(curr_elem, struct user_pte_ptr, elem);
      uint32_t *pte = get_pte(current->pagedir, current->uaddr, false);

      *pte = 0;

      if (curr_exec->read_bytes == 0)
      {
        /* If you don't need to read anything */
        *pte += PF_F;
      }
      else if (curr_exec->start_read % PGSIZE != 0)
      {
        /* If you need to start reading from inside a page, also set 0x400 */
        *pte += (curr_exec->start_read << PGBITS) + PF_F + PAGE_MID_FLAG;
      }
      else
      {
        /* If you need to start reading something from the start of a page,
         also set 0x100 */
        *pte += ((curr_exec->start_read + curr_exec->read_bytes) << PGBITS) + PF_F + FULL_PAGE_FLAG;
      }
      free(current);
    }
    list_remove(&curr_exec->elem);
    free(curr_exec);
  }
  else
  {
    /* Find the first free page using the bitmap. This will be the sector in swap 
    where we should start writing the page to. */
    size_t index_in_swap = bitmap_scan_and_flip(swap_table, 0, 1, false);
    swap_page_write(index_in_swap, f);

    /* Go through all the PTEs that pointed to this frame and set their present bit
    to 0, their swap bit to 1, and set the address bits to contain where in swap 
    the page was written to. Keep the write bit the same. */
    while (!list_empty(&f->user_ptes))
    {
      struct list_elem *curr = list_pop_front(&f->user_ptes);
      struct user_pte_ptr *current = list_entry(curr, struct user_pte_ptr, elem);

      void *pte = get_pte(current->pagedir, current->uaddr, false);
      *(uint32_t *)pte = (((index_in_swap << PGBITS) | PTE_S) & (~PTE_P) | (*(uint32_t *)pte & PTE_W));
      free(current);
    }
  }
  memset(f->kvaddr, 0, PGSIZE);
  /* Set the pin to false */
  f->pin = false;
  lock_release(&f->lock);
}

static int get_user(const uint8_t *uaddr);

/* Read from 8 sectors in swap, since a Page Size = 8 * Block Sector Size */
void swap_page_read(size_t index, void *kvaddr)
{
  int size = 0;
  size_t start_sector = index * PGSIZE / BLOCK_SECTOR_SIZE;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
  {
    block_read(swap, start_sector++, kvaddr + size);
    size += BLOCK_SECTOR_SIZE;
  }
  bitmap_flip(swap_table, index);
}

static bool put_user(uint8_t *udst, uint8_t byte);

/* Takes the first sector in swap where to start writing the page 
contained in frame f. It writes PGSIZE to swap, which is contained in
 PGSIZE/BLOCK_SECTOR_SIZE */
void swap_page_write(size_t index, struct frame *f)
{
  void *kvaddr = f->kvaddr;
  size_t start_sector = index * PGSIZE / BLOCK_SECTOR_SIZE;
  char *buffer = malloc(BLOCK_SECTOR_SIZE);
  if (buffer == NULL)
  {
    lock_release(&f->lock);
    sys_exit_failure();
  }
  int size = 0;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
  {
    char *temp = buffer;
    for (int j = 0; j < BLOCK_SECTOR_SIZE; j++)
    {
      if (!put_user(buffer++, ((char *)kvaddr + size)[j]))
      {
        free(buffer);
        lock_release(&f->lock);
        sys_exit_failure();
      }
    }
    buffer = temp;

    block_write(swap, start_sector++, buffer);
    size += BLOCK_SECTOR_SIZE;
  }
  free(buffer);
}

/* Writes BYTE to user address UDST. UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

/* Reads a byte at user virtual address UADDR. UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault occurred. */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}