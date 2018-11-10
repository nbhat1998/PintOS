#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "../lib/kernel/list.h"

void syscall_init (void);

struct file_container {
    int fd; 
    struct file *f; 

    struct list_elem elem; 
};


#endif /* userprog/syscall.h */
