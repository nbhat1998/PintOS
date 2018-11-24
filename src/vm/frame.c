#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vaddr.h"
#include "palloc.h"

struct frame* create_frame() {
    /* not sure about this 
    if(ptov(pte) == PHYS_BASE) {
        return NULL;
    } */
    
    struct frame* new_frame = malloc(sizeof(struct frame));
    if(new_frame == NULL) {
        return NULL;
        // TODO: might need thread exit here? 
    }
    
    void* page = palloc_get_page(PAL_USER);
    if(page == NULL) {
        return NULL;
    }
    new_frame->page =  page;

    // pagedir_set_page somehow

    list_push_back(&frame_table, &new_frame->elem);
    return new_frame;
}

void remove_frames(uint32_t *physical_memory_key) {
    // TODO: iterate through the list, find every frame and 
    // remove from pagedir entries and from frame_table list
    // then free
}