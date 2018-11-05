#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void sys_halt(uint32_t *args);
void sys_exit(uint32_t *args);
void sys_exec(uint32_t *args);
void sys_wait(uint32_t *args);
void sys_remove(uint32_t *args);
void sys_open(uint32_t *args);
void sys_filesize(uint32_t *args);
void sys_tell(uint32_t *args);
void sys_close(uint32_t *args);
void sys_create(uint32_t *args);
void sys_seek(uint32_t *args);
void sys_read(uint32_t *args);
void sys_write(uint32_t *args);

void(*syscalls[13]) = {
    sys_halt,
    sys_exit,
    sys_exec,
    sys_wait,
    sys_remove,
    sys_open,
    sys_filesize,
    sys_tell,
    sys_close,
    sys_create,
    sys_seek,
    sys_read,
    sys_write
  };

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
  printf("system call!\n");
  uint32_t *args = f->esp + 1;
  (*syscalls[*f->esp])(args);

  //thread_exit(); //?
}

void sys_halt(uint32_t *args)
{

}

void sys_exit(uint32_t *args) {

}

void sys_exec(uint32_t *args) {

}

void sys_wait(uint32_t *args) {

}

void sys_remove(uint32_t *args) {

}

void sys_open(uint32_t *args) {

}

void sys_filesize(uint32_t *args) {

}

void sys_tell(uint32_t *args) {

}

void sys_close(uint32_t *args) {

}

void sys_create(uint32_t *args) {

}

void sys_seek(uint32_t *args) {

}

void sys_read(uint32_t *args) {

}

void sys_write(uint32_t *args) {

}