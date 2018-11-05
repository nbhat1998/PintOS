#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

uint32_t sys_halt(uint32_t *args);
uint32_t sys_exit(uint32_t *args);
uint32_t sys_exec(uint32_t *args);
uint32_t sys_wait(uint32_t *args);
uint32_t sys_create(uint32_t *args);
uint32_t sys_remove(uint32_t *args);
uint32_t sys_open(uint32_t *args);
uint32_t sys_filesize(uint32_t *args);
uint32_t sys_read(uint32_t *args);
uint32_t sys_write(uint32_t *args);
uint32_t sys_seek(uint32_t *args);
uint32_t sys_tell(uint32_t *args);
uint32_t sys_close(uint32_t *args);

uint32_t (*syscalls[13])(uint32_t*) = {
    sys_halt,
    sys_exit,
    sys_exec,
    sys_wait,
    sys_create,
    sys_remove,
    sys_open,
    sys_filesize,
    sys_read,
    sys_write,
    sys_seek,
    sys_tell,
    sys_close
};

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
  printf("system call!\n");
  uint32_t *args = (uint32_t*) f->esp + 1;
  f->eax = syscalls[*((int*) f->esp)](args);
}

uint32_t sys_halt(uint32_t *args)
{
  return 0;
}

uint32_t sys_exit(uint32_t *args)
{
  return 0;
}

uint32_t sys_exec(uint32_t *args)
{
  return 0;
}

uint32_t sys_wait(uint32_t *args)
{
  return 0;
}

uint32_t sys_create(uint32_t *args)
{
  return 0;
}

uint32_t sys_remove(uint32_t *args)
{
  return 0;
}

uint32_t sys_open(uint32_t *args)
{
  return 0;
}

uint32_t sys_filesize(uint32_t *args)
{
  return 0;
}

uint32_t sys_read(uint32_t *args)
{
  return 0;
}

uint32_t sys_write(uint32_t *args)
{
  return 0;
}

uint32_t sys_seek(uint32_t *args)
{
  return 0;
}

uint32_t sys_tell(uint32_t *args)
{
  return 0;
}

uint32_t sys_close(uint32_t *args)
{
  return 0;
}