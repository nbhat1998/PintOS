#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "syscall.h"
#include "pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/share.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#define STACK_LIMIT 0xbfe00000

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void)
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int(5, 3, INTR_ON, kill,
                    "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int(7, 0, INTR_ON, kill,
                    "#NM Device Not Available Exception");
  intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int(19, 0, INTR_ON, kill,
                    "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void)
{
  printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill(struct intr_frame *f)
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
  {
  case SEL_UCSEG:
    /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
    printf("%s: dying due to interrupt %#04x (%s).\n",
           thread_name(), f->vec_no, intr_name(f->vec_no));
    intr_dump_frame(f);
    thread_exit();

  case SEL_KCSEG:
    /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
    intr_dump_frame(f);
    PANIC("Kernel bug - unexpected interrupt in kernel");

  default:
    /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
    printf("Interrupt %#04x (%s) in unknown segment %04x\n",
           f->vec_no, intr_name(f->vec_no), f->cs);
    thread_exit();
  }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault(struct intr_frame *f)
{
  bool not_present; /* True: not-present page, false: writing r/o page. */
  bool write;       /* True: access was write, false: access was read. */
  bool user;        /* True: access by user, false: access by kernel. */
  bool file;        /* True: is file, false: is not file */
  bool mmap;        /* True: is mmap file, false: is not mmap file */
  bool stack;       /* True: is stack page, false: is not stack page */
  bool executable;  /* True: is executable, false: is not executable */
  bool swapped;     /* True: is swapped, false: is not swapped */
  void *fault_addr; /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instructioxn
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm("movl %%cr2, %0"
      : "=r"(fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable();

  /* Count page faults. */
  page_fault_cnt++;

  void *pte = get_pte(thread_current()->pagedir, fault_addr, false);

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* Getting the correct esp to use when checking for stack pages */
  void *esp = f->esp;
  if (!is_user_vaddr(esp))
  {
    esp = thread_current()->process->esp;
  }

  /* Further determination of page fault cause */
  if (pte != NULL && is_user_vaddr(fault_addr) && not_present)
  {
    uint32_t int_pte = (uint32_t)(*((uint32_t *)pte));
    mmap = (int_pte & PF_M) == PF_M;
    stack = fault_addr > STACK_LIMIT && (fault_addr >= esp - (WORD_LENGTH * BYTE)) && ((int_pte & PF_S) == 0);
    executable = (int_pte & PF_F) != 0;
    swapped = !executable && (int_pte & PF_S) != 0;
  }
  else
  {
    mmap = false;
    stack = false;
    executable = false;
    swapped = false;
  }

  if (pte != NULL && is_user_vaddr(fault_addr))
  {
    pagedir_set_accessed(thread_current()->pagedir, fault_addr, true);
  }

  /* -------------------------- MMaped Page -------------------------- */
  /* If there is a page fault at an address that is known to be an mmap'd 
  address, then lazily load the pages from the file into the frame table. 
  The contents of each kvaddr returned by PALLOC_GET_PAGE(PAL_USER) is set 
  to 0 before being read into, so as to enable padding when the page isn't 
  entirely used. If kvaddr is found to be null, a page is evicted from the
  frame table and the same process is carried out. */
  if (mmap)
  {
    void *kvaddr = palloc_get_page(PAL_USER);
    if (kvaddr == NULL)
    {
      kvaddr = evict();
    }
    else
    {
      create_frame(kvaddr);
    }
    bool success = link_page(fault_addr, kvaddr, true);
    set_frame(kvaddr, fault_addr);
    if (!success)
    {
      sys_exit_failure();
      NOT_REACHED();
    }
    pagedir_set_dirty(thread_current()->pagedir, pg_round_down(fault_addr), write);
    for (struct list_elem *e = list_begin(
             &thread_current()->process->mmap_containers);
         e != list_end(&thread_current()->process->mmap_containers);
         e = list_next(e))
    {
      struct mmap_container *this_container =
          list_entry(e, struct mmap_container, elem);
      if (this_container->uaddr == pg_round_down(fault_addr))
      {
        memset(kvaddr, 0, PGSIZE);
        file_read_at(this_container->f, kvaddr,
                     this_container->size_used_within_page,
                     this_container->offset_within_file);

        return;
      }
    }
    sys_exit_failure();
    NOT_REACHED();
  }

  /* -------------------------- Stack Page -------------------------- */
  /* If there was a fault where a stack page was supposed to be, we get a 
  new frame, either by creating a new one or evicting an existing one, 
  and link it to the fault_address. */
  if (stack)
  {
    void *kvaddr = palloc_get_page(PAL_USER);
    if (kvaddr == NULL)
    {
      kvaddr = evict();
    }
    else
    {
      create_frame(kvaddr);
    }

    bool success = link_page(fault_addr, kvaddr, true);
    set_frame(kvaddr, fault_addr);

    if (!success)
    {
      sys_exit_failure();
    }
    return;
  }

  /* -------------------------- Executable Page -------------------------- */
  /* If there was a page fault where an executable page should be, we get the 
  information about where in the file to read from, and how much to read. */
  if (executable)
  {
    uint32_t int_pte = (uint32_t)(*((uint32_t *)pte));
    const int PAGE_MID_FLAG = 0x400;
    const int FULL_PAGE_FLAG = 0x100;
    uint32_t start_read,
        read_bytes;

    if ((int_pte & PAGE_MID_FLAG) != 0)
    {
      /* Middle of a page*/
      start_read = int_pte >> PGBITS;
      read_bytes = PGSIZE - (start_read % PGSIZE);
    }
    else
    {
      /* Start of page */
      start_read = int_pte >> PGBITS;
      if (((int_pte >> PGBITS) % PGSIZE) == 0)
      {
        /* increment of page size */
        if ((int_pte & FULL_PAGE_FLAG) != 0)
        {
          /* Read all of it */
          read_bytes = PGSIZE;
          start_read -= PGSIZE;
        }
        else
        {
          /* read none of it */
          read_bytes = 0;
        }
      }
      else
      {
        /* read amount is mod PGSIZE */
        read_bytes = (int_pte >> PGBITS) % PGSIZE;
        start_read -= read_bytes;
      }
    }

    struct file *file = thread_current()->process->executable;
    int file_size = file_length(file);
    bool rw = (int_pte & PTE_W) != 0;
    void *kvaddr;
    /* If the file is read only, we look to see if it is already in the
    shared_execs list. */
    if (!rw)
    {
      bool found = false;
      struct shared_exec *curr_exec;
      for (struct list_elem *e = list_begin(&shared_execs);
           e != list_end(&shared_execs);
           e = list_next(e))
      {
        curr_exec = list_entry(e, struct shared_exec, elem);
        if (curr_exec->name == &thread_current()->name &&
            curr_exec->start_read == start_read)
        {
          found = true;
          break;
        }
      }
      if (found)
      {
        /* If it is, we link the fault_address (user address) to the address where
        the executable page already exists in memory (kernel address). */
        bool success = link_page(fault_addr, curr_exec->kvaddr, rw);
        set_frame(curr_exec->kvaddr, fault_addr);
        if (!success)
        {
          sys_exit_failure();
        }
        return;
      }
      else
      {
        /* If it isn't, then we get a new page, and we link fault_address to it.
        Then, we create a new struct shared_exec for it. */
        kvaddr = palloc_get_page(PAL_USER);
        if (kvaddr == NULL)
        {
          kvaddr = evict();
        }
        else
        {
          create_frame(kvaddr);
        }

        struct shared_exec *new_exec = malloc(sizeof(struct shared_exec));
        new_exec->name = &thread_current()->name;
        new_exec->kvaddr = kvaddr;
        new_exec->read_bytes = read_bytes;
        new_exec->start_read = start_read;
        list_push_back(&shared_execs, &new_exec->elem);
      }
    }
    else
    {
      /* If it's not read-only, read the executable into a new frame.*/
      kvaddr = palloc_get_page(PAL_USER);
      if (kvaddr == NULL)
      {
        kvaddr = evict();
      }
      else
      {
        create_frame(kvaddr);
      }
    }

    bool success = link_page(fault_addr, kvaddr, rw);
    set_frame(kvaddr, fault_addr);

    int actually_read = 0;
    memset(kvaddr, 0, PGSIZE);
    if (read_bytes != 0)
    {
      if ((int_pte & PAGE_MID_FLAG) != 0)
      {
        actually_read = file_read_at(file, kvaddr + (start_read % PGSIZE), read_bytes, start_read);
      }
      else
      {
        actually_read = file_read_at(file, kvaddr, read_bytes, start_read);
      }
    }

    if (actually_read != read_bytes)
    {
      sys_exit_failure();
    }
    if (!success)
    {
      sys_exit_failure();
    }
    return;
  }

  /* -------------------------- Swapped Page -------------------------- */
  /* If the page was swapped, then call swap_read to get it from memory. */
  if (swapped)
  {
    swap_read(fault_addr);
    return;
  }

  /* Other errors */
  if ((fault_addr < PHYS_BASE && !not_present && write) || user)
  {
    sys_exit_failure();
  }

  if (not_present && fault_addr < PHYS_BASE && !user)
  {
    f->eip = f->eax;
    f->eax = 0xFFFFFFFF;
    sys_exit_failure();
  }

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf("Page fault at %p: %s error %s page in %s context.\n",
         fault_addr,
         not_present ? "not present" : "rights violation",
         write ? "writing" : "reading",
         user ? "user" : "kernel");
  kill(f);
}
