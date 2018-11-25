#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <stdint.h>
#include <bitmap.h>
#include "../devices/block.h"

struct bitmap *swap_map;

void swap_read(void *);

#endif