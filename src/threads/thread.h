#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "filesys/file.h"

#include "../userprog/syscall.h"

/* States in a thread's life cycle. */
enum thread_status
{
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */
#define TIMER_FREQ 100 /* Timer frequency   */
/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
{
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  uint8_t *stack;            /* Saved stack pointer. */

  int init_priority;        /* Initial priority, before donations */
  struct list donations;    /* List of donated priorities */
  struct thread *recipient; /* Pointer to the thread who received a donation from thread 
                                 Only valid if thread has donated to another thread*/

  struct list child_processes; /* List of child processes (struct process), which stores 
                                  all the processes started by this thread */
  struct process *process;     /* Current thread's own struct process */

  struct list_elem allelem; /* List element for all threads list. */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem; /* List element. */

  int nice;           /* Thread's niceness value, used to recalculate thread's priority */
  int priority;       /* Priority. */
  int32_t recent_cpu; /* Estimate of the time taken on the CPU recently*/
#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t *pagedir; /* Page directory. */
#endif

  /* Owned by thread.c. */
  unsigned magic; /* Detects stack overflow. */
};

/* Struct that stores the necessary members for representing and synchronizing a process */
struct process
{
  tid_t pid;                   /* The process ID, which is the same as the tid */
  struct semaphore sema;       /* Semaphore used when a parent thread waits for a child thread */
  struct semaphore setup_sema; /* Semaphore that is upped when loading finishes */
  struct lock lock;            /* Lock used to synchronize operations on struct process */
  int status;                  /* Curret status of the process */
  void *esp;                   /* Current stack pointer */
  struct list_elem elem;       /* List element used to add it to the parent's list of children */
  bool first_done;             /* True when either the parent or the child finish execution */
  bool setup;                  /* True if the process loaded successfully, false otherwise */
  struct list file_containers; /* List of file containers storing information about the files 
                                  opened by this process. */
  struct list mmap_containers; /* List of mmap containers storing informaiton about the pages of the file 
                                  stored in physical memory by this process*/
  struct file *executable;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);
void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void update_priority(struct thread *cur, struct thread *caller,
                     int new_priority);
void thread_yield_cond(void);

bool list_more_priority(const struct list_elem *elem_a,
                        const struct list_elem *elem_b, void *aux);

#endif /* threads/thread.h */
