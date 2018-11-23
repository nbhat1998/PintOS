#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void create_frame(uint32_t *physical_memory_key, uint32_t *uaddr) {
    struct frame* new_frame = malloc(sizeof(struct frame));
    new_frame->physical_memory_key = physical_memory_key;
    new_frame->pagedir = thread_current()->pagedir;
    new_frame->uaddr = uaddr;
    list_push_back(&frame_table, &new_frame->elem);
}

void remove_frames(uint32_t *physical_memory_key) {
    // TODO: iterate through the list, find every frame and 
    // remove from pagedir entries and from frame_table list
    // then free
}