#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name)
{
  char *fn_copy;
  char *file_name_kernel;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
  {
    return TID_ERROR;
  }
  strlcpy(fn_copy, file_name, PGSIZE);

  file_name_kernel = palloc_get_page(0);
  if (file_name_kernel == NULL)
  {
    return TID_ERROR;
  }
  strlcpy(file_name_kernel, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  char *save_ptr;
  char *name = strtok_r(file_name_kernel, " ", &save_ptr);

  tid = thread_create(name, PRI_DEFAULT, start_process, fn_copy);

  palloc_free_page(file_name_kernel);
  if (tid == TID_ERROR)
  {
    palloc_free_page(fn_copy);
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process(void *args)
{
  char *file_name = args;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset(&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Load File and setup stack */
  success = load(file_name, &if_.eip, &if_.esp);

  /* For sys_exec to wakup thread that created this after load is done */
  thread_current()->process->setup = success;
  sema_up(&thread_current()->process->setup_sema);

  /* If load failed, quit. */
  palloc_free_page(file_name);
  if (!success)
  {
    thread_exit();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile("movl %0, %%esp; jmp intr_exit"
               :
               : "g"(&if_)
               : "memory");
  NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting. */
int process_wait(tid_t child_tid)
{
  struct list_elem *child;
  /*  Iterate through the list of child processes to find 
      the process with the corresponding pid (if the child process
      is not found in the list, we return -1) */
  for (child = list_begin(&thread_current()->child_processes);
       child != list_end(&thread_current()->child_processes);
       child = list_next(child))
  {
    struct process *child_process = list_entry(child, struct process, elem);
    lock_acquire(&child_process->lock);
    if (child_process->pid == child_tid)
    {
      /* Check if the child process is not done, and if so, sema_down 
         to wait for it to terminate (the child process will call sema_up 
         upon termination, regardless of how it terminated) */
      if (!child_process->first_done)
      {
        lock_release(&child_process->lock);
        sema_down(&child_process->sema);
        lock_acquire(&child_process->lock);
      }
      /* Otherwise, get the status of the child process,
         remove it from the list, free it, and finally return the status */
      int status = child_process->status;
      list_remove(&child_process->elem);
      lock_release(&child_process->lock);
      free(child_process);
      return status;
    }
    lock_release(&child_process->lock);
  }
  return -1;
}

/* Free the current process's resources. */
void process_exit(void)
{
  struct thread *cur = thread_current();
  uint32_t *pd;

  /* Iterate through the list of child processes */
  struct list_elem *child = list_begin(&cur->child_processes);
  while (child != list_end(&cur->child_processes))
  {
    struct process *child_process = list_entry(child, struct process, elem);
    lock_acquire(&child_process->lock);
    if (!child_process->first_done)
    {
      /* Check if the child process is not done, and if so,
         set the first_done attribute to true to indicate that
         the parent thread (which is the current thread) is done */
      child_process->first_done = true;
      child = list_next(child);
      lock_release(&child_process->lock);
    }
    else
    {
      /* Otherwise, remove it from the list and free it */
      struct list_elem *temp = child;
      child = list_next(child);
      list_remove(temp);
      lock_release(&child_process->lock);
      free(child_process);
    }
  }

  lock_acquire(&cur->process->lock);

  /* Close all files opened by this process */
  struct list_elem *file_elem = list_begin(&cur->process->file_containers);
  while (file_elem != list_end(&cur->process->file_containers))
  {
    struct file_container *file_container = list_entry(file_elem, struct file_container, elem);
    struct list_elem *temp = file_elem;
    file_elem = list_next(file_elem);
    list_remove(temp);
    file_close(file_container->f);
    free(file_container);
  }

  /* Edit current process */
  if (!cur->process->first_done)
  {
    /* Check if the parent thread is not done. If so,
       set the first_done member to true (so the parent 
       can see that the child is done), and then sema_up */
    cur->process->first_done = true;
    sema_up(&cur->process->sema);
    lock_release(&cur->process->lock);
  }
  else
  {
    /* Otherwise just free the process */
    lock_release(&cur->process->lock);
    free(cur->process);
  }

  // struct list_elem *curr = list_begin(&frame_table);
  // for (struct list_elem *curr = list_begin(&frame_table);
  //      curr != list_end(&frame_table); curr = list_next(e))
  // {
  //   struct frame *frame = list_entry(curr, struct frame, elem);
  //   struct list_elem *curr_pte = list_begin(&frame->user_ptes);
  //   for (struct list_elem *curr_pte = list_begin(&(frame->user_ptes));
  //        curr_pte != list_end(&(frame->user_ptes)); curr_pte = list_next(curr))
  //   {
  //     struct user_pte_ptr *user_pte = list_entry(curr_pte, struct user_pte_ptr, elem);
  //     if (user_pte->pagedir == cur->pagedir)
  //     {
  //       list_remove(&curr_pte);
  //       free(user_pte);
  //       break;
  //     }
  //   }
  // }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate(NULL);
    pagedir_destroy(pd);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate(void)
{
  struct thread *t = thread_current();

  /* Activate thread's page tables. */
  pagedir_activate(t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp, const char *argv);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool load(const char *argv, void (**eip)(void), void **esp)
{
  struct thread *t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  char *argv_cpy = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create();
  if (t->pagedir == NULL)
  {
    goto done;
  }
  process_activate();

  /* Open executable file. */
  char *save_ptr;
  argv_cpy = (char *)malloc(strlen(argv) + 1);
  if (argv_cpy == NULL)
  {
    goto done;
  }
  strlcpy(argv_cpy, argv, strlen(argv) + 1);
  char *file_name = strtok_r(argv_cpy, " ", &save_ptr);

  lock_acquire(&filesys_lock);
  file = filesys_open(file_name);
  lock_release(&filesys_lock);

  if (file == NULL)
  {
    printf("load: %s: open failed\n", file_name);
    goto done;
  }

  /* add executable to process */
  lock_acquire(&thread_current()->process->lock);
  thread_current()->process->executable = file;
  lock_release(&thread_current()->process->lock);

  /* Add file to list of file containers and deny write */
  int fd = new_file_container(file);
  file_deny_write(file);

  /* Read and verify executable header. */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
      memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 ||
      ehdr.e_machine != 3 || ehdr.e_version != 1 ||
      ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024)
  {
    printf("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file))
      goto done;
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type)
    {
    case PT_NULL:
    case PT_NOTE:
    case PT_PHDR:
    case PT_STACK:
    default:
      /* Ignore this segment. */
      break;
    case PT_DYNAMIC:
    case PT_INTERP:
    case PT_SHLIB:
      goto done;
    case PT_LOAD:
      if (validate_segment(&phdr, file))
      {
        bool writable = (phdr.p_flags & PF_W) != 0;
        uint32_t file_page = phdr.p_offset & ~PGMASK;
        uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
        uint32_t page_offset = phdr.p_vaddr & PGMASK;
        uint32_t read_bytes, zero_bytes;
        if (phdr.p_filesz > 0)
        {
          /* Normal segment.
                     Read initial part from disk and zero the rest. */
          read_bytes = page_offset + phdr.p_filesz;
          zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
        }
        else
        {
          /* Entirely zero.
                     Don't read anything from disk. */
          read_bytes = 0;
          zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
        }
        if (!load_segment(file, file_page, (void *)mem_page,
                          read_bytes, zero_bytes, writable))
          goto done;
      }
      else
        goto done;
      break;
    }
  }

  /* Set up stack. */
  if (!setup_stack(esp, argv))
  {
    goto done;
  }

  /* Start address. */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  free(argv_cpy);
  // printf("done: %d\n", success);

  return success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
  {
    return false;
  }

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
  {
    return false;
  }

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
  {
    return false;
  }
  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
  {
    return false;
  }

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr((void *)phdr->p_vaddr))
  {
    return false;
  }
  if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
  {
    return false;
  }
  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
  {
    return false;
  }

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
  {
    return false;
  }

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
             uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  file_seek(file, ofs);

  bool first_page = true;

  uint32_t start_read = ofs;

  //printf("ofs %d\n", ofs / PGSIZE);
  while (read_bytes > 0 || zero_bytes > 0)
  {

    /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    uint32_t *pte = get_pte(thread_current()->pagedir, upage, true);
    // printf("uaddr: %p, bool %d\n", upage, writable);
    if (writable)
    {
      *pte = PTE_W;
    }
    else
    {
      *pte = 0;
    }

    // TODO: make this nice
    if (page_read_bytes == 0)
    {
      /* If you don't need to read anything */
      *pte += 0x200;
    }
    else if (start_read % PGSIZE != 0)
    {
      /* If you need to start reading from inside a page, also set 0x400 */
      *pte += (start_read << 12) + 0x600;
    }
    else
    {
      /* If you need to start reading something from the start of a page,
         also set 0x100 */
      *pte += ((start_read + page_read_bytes) << 12) + 0x300;
    }
    //printf("upage %p\n", upage);
    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
    start_read += page_read_bytes;
  }
  // printf("%s: load_segment\n", thread_current()->name);

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack(void **esp, const char *argv)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage == NULL)
  {
    kpage = evict();
  }
  else
  {
    create_frame(kpage);
  }
  success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
  set_frame(kpage, ((uint8_t *)PHYS_BASE) - PGSIZE);

  if (success)
  {
    *esp = PHYS_BASE;
  }
  else
  {
    palloc_free_page(kpage);
  }

  uint8_t *sp = PHYS_BASE;

  /* Tokenize argv to get the arguments and write then to stack. */
  char *token, *save_ptr;
  int argc = 0;
  for (token = strtok_r(argv, " ", &save_ptr); token != NULL;
       token = strtok_r(NULL, " ", &save_ptr))
  {
    argc++;
    size_t token_length = strlen(token) + 1;
    sp -= (int8_t)(token_length);
    strlcpy(sp, token, token_length);
  }

  /* Word align */
  int8_t *argv_ptr = sp;
  sp -= (uint8_t)sp % 4;

  /* Add null pointer (end of argv) */
  sp -= 4;
  *sp = NULL;

  /* Adding argv addresses,address of ar0gv array, argc,
     and the return address to stack */
  int32_t *sp32 = (int32_t *)sp;
  for (int i = 0; i <= argc; i++)
  {
    *(--sp32) = argv_ptr;

    while (*argv_ptr != '\0')
    {
      argv_ptr++;
    }
    argv_ptr++;
  }

  /* Add the address of the last argument added and
     the number of arguments */
  *(--sp32) = sp32 + 1;
  *(--sp32) = argc;
  *(--sp32) = 0;

  *esp = (void *)sp32;

  return success;
}
bool link_page(void *upage, void *kpage, bool writable);
/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

bool link_page(void *upage, void *kpage, bool writable)
{
  return install_page(pg_round_down(upage), kpage, writable);
}