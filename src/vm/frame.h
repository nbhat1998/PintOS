#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>

#define MAX_FRAMES 1 << 16

struct frame {
    void* page;
    // TODO: additional info of choice
    // TODO: maybe have a flag if it's a file and what file it is
    struct list_elem elem;
};

struct list frame_table;

struct frame* create_frame();
void remove_frames(uint32_t *physical_memory_key);

#endif /* vm/frame.h */
