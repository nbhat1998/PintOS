#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "../filesys/filesys.h"


/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

#define WORD_LENGTH 4
#define BYTE 8
static int32_t 
get_word(uint8_t *uaddr) 
{
  int32_t word = 0;

  for(int i = 0; i < WORD_LENGTH; i++) 
  {
    int new_byte = 0;
    new_byte = get_user(uaddr);
    new_byte <<= i * BYTE;
    word += new_byte;
    uaddr++;
  }

  return word;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

/* Lock used by filesystem */
// TODO: LOCK INIT SOMEWHERE...
static struct lock filesys_lock;

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

uint32_t (*syscalls[13])(uint32_t *) = {
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
    sys_close};

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int
allocate_fd(void)
{
  static int next_fd = 2;
  int fd;

  //TODO: USE A DIFFERENT LOCK!!!!!! (global)
  lock_acquire(&filesys_lock);
  fd = next_fd++;
  lock_release(&filesys_lock);

  return fd;
}


static void
syscall_handler(struct intr_frame *f)
{
  printf("system call!\n");
  int function = get_word(f->esp);
  if(function == -1) {
    // TODO: DO SOMETHING BAD
  }


  uint32_t *args = (uint32_t *)f->esp + 1;
  hex_dump(0, args, (int8_t *) PHYS_BASE - ((int8_t*) args), true);

  f->eax = syscalls[function](args);
}

uint32_t sys_halt(uint32_t *args)
{
  return 0;
}

uint32_t sys_exit(uint32_t *args)
{
  // thread_current()->process->status = 
  thread_exit();
  NOT_REACHED();
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
  char* file = get_word(args);
  char* file_name;
  strlcpy(file_name, file, sizeof(file));

  lock_acquire(&filesys_lock);
  struct file_container* new_file = malloc(sizeof (struct file_container));
  new_file->fd = allocate_fd();
  new_file->f = filesys_open(file_name);
  list_push_back(&thread_current()->process->file_containers, &new_file->elem);
  lock_release(&filesys_lock);

  return new_file->fd;
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