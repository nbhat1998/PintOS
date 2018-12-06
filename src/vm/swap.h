#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include "vm/frame.h"

struct bitmap *swap_table; /* Bitmap used for swap table */

void swap_read(void *fault_addr);
void swap_write(struct frame *f);

#endif
