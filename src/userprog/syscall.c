#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/string.h"

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

  for (int i = 0; i < WORD_LENGTH; i++)
  {
    if (uaddr >= PHYS_BASE)
    {
      sys_exit_failure();
    }
    int new_byte = 0;
    new_byte = get_user(uaddr);
    if (new_byte == -1)
    {
      sys_exit_failure();
    }
    new_byte <<= i * BYTE;
    word += new_byte;
    uaddr++;
  }

  return word;
}

static bool
check_ptr(uint8_t *uaddr)
{
  char c;
  do
  {
    c = get_user(uaddr);
    if (uaddr >= PHYS_BASE || c == -1)
    {
      return false;
    }
    uaddr++;
  } while (c != '\0');
  return true;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

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

int allocate_fd()
{
  static int next_fd = 2;
  int fd;

  lock_acquire(&filesys_lock);
  fd = next_fd++;
  lock_release(&filesys_lock);

  return fd;
}

static void
syscall_handler(struct intr_frame *f)
{
  int function = get_word(f->esp);
  if (function == NULL)
  {
    sys_exit_failure();
  }

  uint32_t *args = (uint32_t *)f->esp + 1;

  f->eax = syscalls[function](args);
}

uint32_t sys_halt(uint32_t *args)
{
  shutdown_power_off();
  // good night sweet prince
  NOT_REACHED();
  return 0;
}

void sys_exit_failure()
{
  //thread_current()->process->status = -1;
  printf("%s: exit(%d)\n", thread_current()->name, thread_current()->process->status);
  thread_exit();
  NOT_REACHED();
}

uint32_t sys_exit(uint32_t *args)
{
  thread_current()->process->status = get_word(args);
  printf("%s: exit(%d)\n", thread_current()->name, thread_current()->process->status);
  thread_exit();
  NOT_REACHED();
  return 0;
}

uint32_t sys_exec(uint32_t *args)
{
  char *name = get_word(args);
  if (!check_ptr(name))
  {
    sys_exit_failure();
  }

  tid_t tid = process_execute(name);
  if (tid == -1)
  {
    return -1;
  }
  /* Wait until setup is done */
  struct list_elem *child;
  for (child = list_begin(&thread_current()->child_processes);
       child != list_end(&thread_current()->child_processes);
       child = list_next(child))
  {
    struct process *child_process = list_entry(child, struct process, elem);
    lock_acquire(&child_process->lock);
    if (child_process->pid == tid)
    {
      lock_release(&child_process->lock);
      sema_down(&child_process->setup_sema);
      lock_acquire(&child_process->lock);
      if (child_process->setup)
      {
        lock_release(&child_process->lock);
        return tid;
      }
      else
      {
        lock_release(&child_process->lock);
        return -1;
      }
    }
    lock_release(&child_process->lock);
  }
}

uint32_t sys_wait(uint32_t *args)
{
  tid_t param_pid = (tid_t)get_word(args);
  return process_wait(param_pid);
}

uint32_t sys_create(uint32_t *args)
{
  char *file = get_word(args);

  if (!check_ptr(file))
  {
    sys_exit_failure();
  }

  args++;
  unsigned initial_size = get_word(args);

  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  return success;
}

uint32_t sys_remove(uint32_t *args)
{
  char *file = get_word(args);
  if (!check_ptr(file))
  {
    sys_exit_failure();
  }
  char *file_name = malloc(PGSIZE);
  if (file_name == NULL)
  {
    return -1;
  }
  strlcpy(file_name, file, PGSIZE);
  lock_acquire(&filesys_lock);

  bool success = filesys_remove(file);
  lock_release(&filesys_lock);
  free(file_name);
  return success;
}

uint32_t sys_open(uint32_t *args)
{
  char *file = get_word(args);

  if (!check_ptr(file))
  {
    sys_exit_failure();
  }

  char *file_name = malloc(PGSIZE);
  if (file_name == NULL)
  {
    return -1;
  }

  strlcpy(file_name, file, PGSIZE);

  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file_name);
  lock_release(&filesys_lock);

  if (f == NULL)
  {
    free(file_name);
    return -1;
  }

  int fd = new_file_container(f);

  free(file_name);

  return fd;
}

int new_file_container(struct file *file)
{
  struct file_container *new_file = malloc(sizeof(struct file_container));
  if (new_file == NULL)
  {
    return -1;
  }
  new_file->fd = allocate_fd();

  new_file->f = file;
  list_push_back(&thread_current()->process->file_containers, &new_file->elem);
  return new_file->fd;
}

uint32_t sys_filesize(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  int length_of_file;
  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers); e != list_end(&thread_current()->process->file_containers); e = list_next(e))
  {
    struct file_container *this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      length_of_file = file_length(this_container->f);
      lock_release(&filesys_lock);
      return length_of_file;
    }
  }
  return -1;
}

uint32_t sys_seek(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args = args + 1;

  unsigned param_position = (unsigned)get_word(args);

  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
                         e != list_end(&thread_current()->process->file_containers);
                         e = list_next(e))
  {
    struct file_container *this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      file_seek(this_container->f, param_position);
      lock_release(&filesys_lock);
      return;
    }
  }
  return;
}

uint32_t sys_tell(uint32_t *args)
{
  int param_fd = (int)get_word(args);

  unsigned tell_value;
  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
                         e != list_end(&thread_current()->process->file_containers);
                         e = list_next(e))
  {
    struct file_container *this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      tell_value = file_tell(this_container->f);
      lock_release(&filesys_lock);
      return tell_value;
    }
  }

  return -1;
}

uint32_t sys_read(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args++;

  char *param_buffer = get_word(args);
  args++;

  unsigned param_size = (unsigned)get_word(args);
  if (param_buffer + param_size >= PHYS_BASE)
  {
    sys_exit_failure();
  }

  int actually_read = 0;
  if (param_fd == 0)
  {
    while (param_size-- > 0)
    {
      char read_value = (char)input_getc();
      if (!put_user(param_buffer++, read_value))
      {
        return 0;
      }
      actually_read++;
    }
  }
  else
  {
    lock_acquire(&filesys_lock);
    for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
         e != list_end(&thread_current()->process->file_containers);
         e = list_next(e))
    {
      struct file_container *this_container = list_entry(e, struct file_container, elem);
      if (param_fd == this_container->fd)
      {
        char *kernel_buffer = malloc(param_size);
        actually_read = file_read(this_container->f, kernel_buffer, param_size);
        for (int i = 0; i < param_size; i++)
        {
          if (!put_user(param_buffer++, kernel_buffer[i]))
          {
            lock_release(&filesys_lock);
            return 0;
          }
        }
        break;
      }
    }
    lock_release(&filesys_lock);
  }

  return actually_read;
}

uint32_t sys_write(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args++;

  char *param_buffer = get_word(args);

  args++;

  unsigned param_size = (unsigned)get_word(args);
  int32_t actually_written = 0;

  if (param_buffer + param_size >= PHYS_BASE)
  {
    return 0;
  }

  if (param_fd == 1)
  {
    if (strlen(param_buffer) < 500)
    {
      lock_acquire(&filesys_lock);
      putbuf(param_buffer, strlen(param_buffer));
      lock_release(&filesys_lock);
      actually_written = strlen(param_buffer);
      return actually_written;
    }
  }
  else
  {
    lock_acquire(&filesys_lock);
    for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
         e != list_end(&thread_current()->process->file_containers);
         e = list_next(e))
    {
      struct file_container *this_container = list_entry(e, struct file_container, elem);
      if (param_fd == this_container->fd)
      {
        actually_written = file_write(this_container->f, param_buffer, param_size);
        lock_release(&filesys_lock);
        return actually_written;
      }
    }
    lock_release(&filesys_lock);
    return 0;
  }
}

uint32_t sys_close(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers); e != list_end(&thread_current()->process->file_containers); e = list_next(e))
  {
    struct file_container *this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      list_remove(e);
      free(this_container);
      lock_release(&filesys_lock);
    }
    return;
  }
}
