#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <list.h>
#include <stdint.h>

struct frame {
    uint32_t *physical_memory_key;
    // TODO: maybe have a flag if it's a file and what file it is
    struct list user_ptes; 
    struct list_elem elem;
};

struct user_pte_ptr {
    uint32_t *pagedir;
    uint32_t *uaddr;
    struct list_elem elem;
}; 

struct list frame_table;

void create_frame(uint32_t *physical_memory_key, uint32_t *uaddr);
void remove_frames(uint32_t *physical_memory_key);

#endif /* vm/frame.h */
