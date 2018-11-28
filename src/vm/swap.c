#include "vm/swap.h"
#include "threads/thread.h"
#include "pte.h"
#include "vm/frame.h"
#include "devices/block.h"


void swap_read(void *fault_addr)
{
  uint32_t pdi = pd_no(fault_addr);
  uint32_t pti = pt_no(fault_addr);
  uint32_t *pte = ptov((thread_current()->pagedir[pdi]) & PTE_ADDR) + pti;
  block_sector_t swap_index = (PTE_ADDR & *pte);
  // TODO: check if always init
  struct block *swap = block_get_role(BLOCK_SWAP);
  /* Either gets freed if frame table is full, or when 
       the whole frame is freed */
  struct frame *new_frame = create_frame();
  if (new_frame == NULL)
  {
    // TODO: eviction
  }

  new_frame->page = malloc(PGSIZE);
  block_read(swap, swap_index, new_frame->page);
  // TODO: add additional info as well
}
