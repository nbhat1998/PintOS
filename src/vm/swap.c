#include "vm/swap.h"
#include "threads/thread.h"
#include "pte.h"
#include "vm/frame.h"
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "lib/string.h"

/* Helpers */
void swap_page_read(block_sector_t start_sector, void *kvaddr);
void swap_page_write(block_sector_t start_sector, void *kvaddr);

void swap_read(struct frame *f)
{
  uint32_t *pte = get_pte(thread_current()->pd, f->vaddr, false);
  block_sector_t swap_index = (PTE_ADDR & *pte);
  // TODO: check if always init
  struct block *swap = block_get_role(BLOCK_SWAP);
  /* Either gets freed if frame table is full, or when 
       the whole frame is freed */
  struct frame *new_frame = palloc_get_page(PAL_USER);

  new_frame->page = malloc(PGSIZE);
  block_read(swap, swap_index, new_frame->page);
  // TODO: add additional info as well
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
  block_sector_t sector = bitmap_scan(swap_table, 0, 8, false);
  swap_page_write(sector, fus->vaddr);
  for (struct list_elem *e = list_begin(&f->user_ptes);
       e != list_end(&f->user_ptes); e = list_next(e))
  {
    struct user_pte_ptr *curr = list_entry(e, struct user_pte_ptr, elem);
    uint32_t *pte =
  }
}

/* Helpers */

void swap_page_read(block_sector_t start_sector, void *kaddr)
{
  char *buffer = malloc(BLOCK_SECTOR_SIZE);
  int size = 0;
  struct block *swap = block_get_role(BLOCK_SWAP);
  for (int i = 0; i < 8; i++)
  {
    block_read(swap, start_sector++, buffer);
    strlcpy((char *)kaddr + size, buffer, BLOCK_SECTOR_SIZE);
    size += BLOCK_SECTOR_SIZE;
  }
  free(buffer);
}

void swap_page_write(block_sector_t start_sector, void *kaddr)
{
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