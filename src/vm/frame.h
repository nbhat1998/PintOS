#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>

struct frame {
    uint32_t *physical_memory_key;
    uint32_t *pagedir;
    uint32_t *uaddr;
    // TODO: maybe have a flag if it's a file and what file it is
    struct list_elem elem;
};

struct list frame_table;

void create_frame(uint32_t *physical_memory_key, uint32_t *uaddr);
void remove_frames(uint32_t *physical_memory_key);

#endif /* vm/frame.h */
