#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <syscall.h>
#include <list.h>

#include "threads/synch.h"
#define WORD_LENGTH 4
#define BYTE 8
#define NUMBER_OF_FUNCTIONS 15
#define ARBITRARY_LENGTH_LIMIT 500
#define FIRST_UNALLOCATED_FD 2

void syscall_init(void);

struct file_container
{
  int fd;
  struct file *f;
  struct list_elem elem;
};

struct mmap_container 
{
  struct file *f; 
  mapid_t mapid; 
  void* uaddr; 
  void* vaddr; 
  uint32_t offset_within_file; 
  uint32_t size_used_within_page; 

}; 

struct lock filesys_lock;

void sys_exit_failure(void);
int new_file_container(struct file *);

#endif /* userprog/syscall.h */
