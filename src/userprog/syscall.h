#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);


struct fileContainer {
    int fd; 
    file *f; 

    struct list_elem elem; 
}


#endif /* userprog/syscall.h */
