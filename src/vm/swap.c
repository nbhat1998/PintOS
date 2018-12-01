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

/* Helpers */
void swap_page_read(size_t index, void *kvaddr);
void swap_page_write(size_t index, void *kvaddr);

void swap_read(void *fault_addr)
{
  uint32_t *pte = get_pte(thread_current()->pagedir, fault_addr, false);
  size_t swap_index = (PTE_ADDR & *pte) >> 12;
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

  set_frame(kvaddr, fault_addr);
  bool success = link_page(fault_addr, kvaddr, writable);

  *pte &= (~0x700); 
  *pte &= (~PTE_S);
  *pte += PTE_P;

  swap_page_read(swap_index, kvaddr);
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
  size_t index_in_swap = bitmap_scan_and_flip(swap_table, 0, 1, false);
  swap_page_write(index_in_swap, f->vaddr);

  while (!list_empty(&f->user_ptes))
  {
    struct list_elem *curr = list_pop_front(&f->user_ptes);
    struct user_pte_ptr *current = list_entry(curr, struct user_pte_ptr, elem);
    uint32_t *pte = get_pte(current->pagedir, current->uaddr, false);
    bool is_file = (*pte & 0x200) != 0;
    *pte = (((index_in_swap << 12) + PTE_S) & (~PTE_P));
    if(is_file) {
      *pte |= 0x700;
    }
    free(current);
  }
}

void swap_page_read(size_t index, void *kaddr)
{
  char *buffer = malloc(BLOCK_SECTOR_SIZE);
  int size = 0;
  size_t start_sector = ((index / PGSIZE) -1 )* 8;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < 8; i++)
  {
    block_read(swap, start_sector++, buffer);
    strlcpy((char *)kaddr + size, buffer, BLOCK_SECTOR_SIZE);
    size += BLOCK_SECTOR_SIZE;
  }
  bitmap_flip(swap_table, (index / PGSIZE));
  free(buffer);
}

void swap_page_write(size_t index, void *kaddr)
{
  size_t start_sector = index * 8;
  char *buffer = malloc(BLOCK_SECTOR_SIZE);
  int size = 0;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < 8; i++)
  {
    strlcpy(buffer, (char *)kaddr + size, BLOCK_SECTOR_SIZE);
    block_write(swap, start_sector++, buffer);
    size += BLOCK_SECTOR_SIZE;
  }
  free(buffer);
}