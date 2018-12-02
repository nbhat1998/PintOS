#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/pte.h"
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
#include "userprog/pagedir.h"

static int get_user(const uint8_t *);
static int32_t get_word(uint8_t *);
static bool check_ptr(uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
int allocate_fd();
int new_file_container(struct file *);

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
uint32_t sys_mmap(uint32_t *args);
uint32_t sys_munmap(uint32_t *args);

uint32_t (*syscalls[NUMBER_OF_FUNCTIONS])(uint32_t *) = {
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
    sys_close,
    sys_mmap,
    sys_munmap};

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Passes control to the respective function, based on the first argument/ word */
static void
syscall_handler(struct intr_frame *f)
{
  int function = get_word(f->esp);
  if (function == NULL)
  {
    return -1;
  }

  uint32_t *args = (uint32_t *)f->esp + 1;

  f->eax = syscalls[function](args);
}

uint32_t
sys_halt(uint32_t *args)
{
  shutdown_power_off();
  NOT_REACHED();
  return 0;
}

void sys_exit_failure()
{
  printf("%s: exit(%d)\n", thread_current()->name, thread_current()->process->status);
  thread_exit();
  NOT_REACHED();
}

uint32_t
sys_exit(uint32_t *args)
{
  thread_current()->process->status = get_word(args);
  printf("%s: exit(%d)\n", thread_current()->name, thread_current()->process->status);
  thread_exit();
  NOT_REACHED();
  return 0;
}

uint32_t
sys_exec(uint32_t *args)
{
  char *name = get_word(args);
  if (!check_ptr(name))
  {
    return -1;
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

uint32_t
sys_wait(uint32_t *args)
{
  tid_t param_pid = (tid_t)get_word(args);
  return process_wait(param_pid);
}

uint32_t
sys_create(uint32_t *args)
{
  char *file = get_word(args);

  if (!check_ptr(file))
  {
    return -1;
  }

  args++;
  unsigned initial_size = get_word(args);

  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  return success;
}

uint32_t
sys_remove(uint32_t *args)
{
  char *file = get_word(args);
  if (!check_ptr(file))
  {
    return -1;
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

uint32_t
sys_open(uint32_t *args)
{
  char *file = get_word(args);

  if (!check_ptr(file))
  {
    return -1;
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
  free(file_name);

  if (f == NULL)
  {
    return -1;
  }

  int fd = new_file_container(f);

  return fd;
}

uint32_t
sys_filesize(uint32_t *args)
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

uint32_t
sys_seek(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args++;

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

uint32_t
sys_tell(uint32_t *args)
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

uint32_t
sys_read(uint32_t *args)
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
            free(kernel_buffer);
            return 0;
          }
        }
        free(kernel_buffer);
        break;
      }
    }
    lock_release(&filesys_lock);
  }

  return actually_read;
}

uint32_t
sys_write(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args++;

  char *param_buffer = get_word(args);
  args++;

  unsigned param_size = (unsigned)get_word(args);
  if (param_size == 0)
  {
    return 0;
  }
  int32_t actually_written = 0;

  if (param_buffer + param_size >= PHYS_BASE)
  {
    return 0;
  }

  char *kernel_buffer = malloc(param_size);
  if (kernel_buffer == NULL)
  {
    sys_exit_failure();
  }
  for (int i = 0; i < param_size; i++)
  {
    int c = get_user(param_buffer++);
    if (c == -1)
    {
      free(kernel_buffer);
      return 0;
    }
    kernel_buffer[i] = (char)c;
  }

  if (param_fd == 1)
  {
    if (param_size < ARBITRARY_LENGTH_LIMIT)
    {
      lock_acquire(&filesys_lock);
      putbuf(kernel_buffer, param_size);
      actually_written = param_size;
      free(kernel_buffer);
      lock_release(&filesys_lock);
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
        actually_written = file_write(this_container->f, kernel_buffer, param_size);
        free(kernel_buffer);
        lock_release(&filesys_lock);
        return actually_written;
      }
    }
    free(kernel_buffer);
    lock_release(&filesys_lock);
    return 0;
  }
}

uint32_t
sys_close(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
       e != list_end(&thread_current()->process->file_containers);
       e = list_next(e))
  {
    struct file_container *this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      list_remove(e);
      file_close(this_container->f);
      free(this_container);
      lock_release(&filesys_lock);
      return;
    }
  }
  return;
}

uint32_t sys_mmap(uint32_t *args)
{
  int param_fd = (int)get_word(args);
  args++;
  if (param_fd < 2)
  {
    return -1;
  }

  uint32_t *uaddr = get_word(args);

  if (uaddr == 0 || ((uint32_t)uaddr % PGSIZE) != 0)
  {
    return -1;
  }

  uint32_t file_size = 0;

  uint32_t *pte = get_pte(thread_current()->pagedir, uaddr, true);

  struct file_container *this_container = NULL;
  for (struct list_elem *e = list_begin(&thread_current()->process->file_containers);
       e != list_end(&thread_current()->process->file_containers);
       e = list_next(e))
  {
    this_container = list_entry(e, struct file_container, elem);
    if (param_fd == this_container->fd)
    {
      lock_acquire(&filesys_lock);
      file_size = file_length(this_container->f);
      lock_release(&filesys_lock);
      break;
    }
  }

  if (file_size == 0)
  {
    return -1;
  }

  uint32_t number_of_pages = file_size / PGSIZE;

  if (file_size % PGSIZE)
  {
    number_of_pages++;
  }

  for (int i = 0; i < number_of_pages; i++)
  {
    uint32_t *current_pte = get_pte(thread_current()->pagedir, uaddr + i * PGSIZE, false);

    if (current_pte != NULL && (*current_pte) != 0)
    {
      return -1;
    }
  }

  int new_mapId = allocate_fd();
  for (int i = 0; i < number_of_pages; i++)
  {
    uint32_t *current_pte = get_pte(thread_current()->pagedir, uaddr + i * PGSIZE, true);

    (*current_pte) = 0x500;
    struct mmap_container *mmap_container = malloc(sizeof(struct mmap_container));
    mmap_container->f = this_container->f;
    mmap_container->mapid = new_mapId;
    mmap_container->uaddr = uaddr;
    mmap_container->offset_within_file = PGSIZE * i;

    if (file_size < PGSIZE)
    {
      mmap_container->size_used_within_page = file_size;
    }
    else
    {
      mmap_container->size_used_within_page = PGSIZE;
      file_size -= PGSIZE;
    }

    list_push_back(&thread_current()->process->mmap_containers, &mmap_container->elem);
  }

  return new_mapId;
}

uint32_t sys_munmap(uint32_t *args)
{
  mapid_t id = get_word(args);

  struct list_elem *e = list_begin(&thread_current()->process->mmap_containers);
  while (e != list_end(&thread_current()->process->mmap_containers))
  {

    struct mmap_container *this_container = list_entry(e, struct mmap_container, elem);
    if (id == this_container->mapid)
    {
      uint32_t *pte = get_pte(thread_current()->pagedir, this_container->uaddr, false);
      if (pte == NULL || *pte == 0)
      {
        return -1;
      }

      if ((*pte) & PTE_D != 0 && ((*pte) & 0x500) != 0x500)
      {
        lock_acquire(&filesys_lock);
        file_write_at(this_container->f, ptov((*pte) & PTE_ADDR),
                      this_container->size_used_within_page,
                      this_container->offset_within_file);
        lock_release(&filesys_lock);
      }
      struct list_elem *temp = e;
      e = list_next(e);
      list_remove(temp);
      free(this_container);
      // TODO : look into removing frame from frame table to free up kvm
    }
    else
    {
      e = list_next(e);
    }
  }
}

/* Reads a byte at user virtual address UADDR. UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault occurred. */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

/* Gets a word by applying get_user() in order to read 4 bytes */
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

/* Checks if each character of a name is user addressable */
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

/* Writes BYTE to user address UDST. UDST must be below PHYS_BASE.
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

/* Atomically returns a unique file descriptor for each new file */
int allocate_fd()
{
  static int next_fd = FIRST_UNALLOCATED_FD;
  int fd;

  lock_acquire(&filesys_lock);
  fd = next_fd++;
  lock_release(&filesys_lock);

  return fd;
}

/* Get a file and stores it in a file container which is then added to the process's
   list of file containers */
int new_file_container(struct file *file)
{
  struct file_container *new_file = malloc(sizeof(struct file_container));
  if (new_file == NULL)
  {
    return -1;
  }
  new_file->fd = allocate_fd();

  new_file->f = file;
  lock_acquire(&thread_current()->process->lock);
  list_push_back(&thread_current()->process->file_containers, &new_file->elem);
  lock_release(&thread_current()->process->lock);

  return new_file->fd;
}