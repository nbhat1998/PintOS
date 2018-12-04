#include "vm/swap.h"
#include "vm/frame.h"
#include <bitmap.h>
#include <string.h>
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/malloc.h"

/* Helpers */
void swap_page_read(size_t index, void *kvaddr);
void swap_page_write(size_t index, struct frame *f);

void swap_read(void *fault_addr)
{
  uint32_t *pte = get_pte(thread_current()->pagedir, fault_addr, false);

  size_t swap_index = (*pte & PTE_ADDR) >> 12;
  bool writable = (*pte & PTE_W) != 0;
  // TODO: check if always init
  /* Either gets freed if frame table is full, or when 
       the whole frame is freed */
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
    if (frame->vaddr == kvaddr)
    {
      lock_release(&frame->lock);
      break;
    }
    lock_release(&frame->lock);
  }

  //printf("read : (thread: %d ) UADDR %p, pte %p\n", thread_current()->tid, fault_addr, *pte);

  lock_acquire(&frame->lock);
  swap_page_read(swap_index, kvaddr);
  lock_release(&frame->lock);

  *pte &= (~PTE_S);
}

/* TODO: 
decide where in block swap to put the stuff ( using bitmap )

f->physicalmemorykey needs to be move to swap block at index decided above 
    use f->physicalmemoryley as void* buffer ( the third param ) 
    use PGSIZE as block_sector
    
f->user_ptes : all of the ptes need to go into the pagedir->pagetable->pte 
        and set preset bit to 0, swap bit to 1, addr = position in swap_table ( index decided above)
*/

void swap_write(struct frame *f)
{
  lock_acquire(&f->lock);
  size_t index_in_swap = bitmap_scan_and_flip(swap_table, 0, 1, false);
  swap_page_write(index_in_swap, f);

  while (!list_empty(&f->user_ptes))
  {
    struct list_elem *curr = list_pop_front(&f->user_ptes);
    struct user_pte_ptr *current = list_entry(curr, struct user_pte_ptr, elem);

    uint32_t *pte = get_pte(current->pagedir, current->uaddr, false);
    bool is_file = (*pte & 0x200) != 0;
    *pte = (((index_in_swap << 12) | PTE_S) & (~PTE_P) | (*pte & PTE_W));
    free(current);
  }
  memset(f->vaddr, 0, PGSIZE);
  f->pin = false;
  lock_release(&f->lock);
}

static int get_user(const uint8_t *uaddr);

void swap_page_read(size_t index, void *kaddr)
{
  int size = 0;
  size_t start_sector = index * 8;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < 8; i++)
  {
    block_read(swap, start_sector++, kaddr + size);
    size += BLOCK_SECTOR_SIZE;
  }
  bitmap_flip(swap_table, index);
}

static bool put_user(uint8_t *udst, uint8_t byte);

void swap_page_write(size_t index, struct frame *f)
{
  void *kaddr = f->vaddr;
  size_t start_sector = index * 8;
  char *buffer = malloc(BLOCK_SECTOR_SIZE);
  if (buffer == NULL)
  {
    lock_release(&f->lock);
    sys_exit_failure();
  }
  int size = 0;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < 8; i++)
  {
    //strlcpy(buffer, (char *)(kaddr + size), BLOCK_SECTOR_SIZE);
    char *temp = buffer;
    for (int j = 0; j < BLOCK_SECTOR_SIZE; j++)
    {
      if (!put_user(buffer++, ((char *)kaddr + size)[j]))
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