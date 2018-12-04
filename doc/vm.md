<h1 align="center" style="white-space:pre">
OS 211 
TASK 3: VIRTUAL MEMORY
DESIGN DOCUMENT
</h1>

#  Pintos Group 13

- Niranjan Bhat <nb1317@ic.ac.uk>  
- Iulia Ivana <imi17@ic.ac.uk>  
- George Soteriou <gs2617@ic.ac.uk>  
- Maria Teguiani <mt5217@ic.ac.uk>  

#  PAGE TABLE/FRAME MANAGEMENT

## DATA STRUCTURES  

> A1: (2 marks)
> Copy here the declaration of each new or changed 'struct' or
> 'struct' member, global or static variable, 'typedef', or
> enumeration that relates to your supplemental page table and
> frame table. Identify the purpose of each in roughly 25 words.

```
<<<<<< frame.h >>>>>>

#define MAX_FRAMES 1 << 16

struct frame
{
  uint32_t *vaddr;
  struct list user_ptes;
  struct list_elem elem;
  bool pin;
};

struct user_pte_ptr
{
  uint32_t *pagedir;
  uint32_t *uaddr;
  struct list_elem elem;
};

struct list frame_table;

```


## ALGORITHMS  

> A2: (2 marks) 
> Describe your code for finding the frame (if any) or other location that 
> contains the data of a given page.

> A3: (2 marks)
> How have you implemented sharing of read only pages?

## SYNCHRONIZATION  

> A4: (2 marks) 
> When two user processes both need a new frame at the same time,
> how are races avoided? You should consider both when there are
> and are not free frames available in memory.

## RATIONALE  

> A5: (2 marks)
> Why did you choose the data structure(s) that you did for
> representing the supplemental page table and frame table?




#  PAGING TO AND FROM DISK

## DATA STRUCTURES  

> B1: (1 mark)
> Copy here the declaration of each new or changed 'struct' or
> 'struct' member, global or static variable, 'typedef', or
> enumeration that relates to your swap table.  
> Identify the purpose of each in roughly 25 words.


```
<<<<<< thread.h >>>>>>

struct process 
{
  ...
  struct file *executable;
};


<<<<<< swap.h >>>>>>

struct bitmap *swap_table;


<<<<<< pte.h >>>>>>

#define PTE_S 0x100          /* 1=swapped, 0=not in swap */


<<<<<< exception.h >>>>>>

#define PF_S 0x100 /* 0: not in swap table, 1: in swap table */
#define PF_F 0x200 /* 0: not a file, 1: file */



```

## ALGORITHMS  

> B2: (2 marks)
> When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

> B3: (2 marks)
> When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page directory (and any other 
> data structures) to reflect the frame Q no longer has?

## SYNCHRONIZATION  

> B4: (2 marks)
> Explain how your synchronization design prevents deadlock.  
> (You may want to refer to the necessary conditions for deadlock.)

> B5: (2 marks)
> A page fault in process P can cause another process Q's frame
> to be evicted. How do you ensure that Q cannot access or modify
> the page during the eviction process?

> B6: (2 marks)
> A page fault in process P can cause another process Q's frame
> to be evicted. How do you avoid a race between P evicting Q's 
> frame and Q faulting the page back in?

> B7: (2 marks)
> Explain how you handle access to paged-out user pages that 
> occur during system calls. 

## RATIONALE  

> B8: (2 marks)
> There is an obvious trade-off between parallelism and the complexity
> of your synchronisation methods. Explain where your design falls
> along this continuum and why you chose to design it this way.




#  MEMORY MAPPED FILES

## DATA STRUCTURES  

> C1: (1 mark)
> Copy here the declaration of each new or changed 'struct' or
> 'struct' member, global or static variable, 'typedef', or
> enumeration that relates to your file mapping table.  
> Identify the purpose of each in roughly 25 words.


```
<<<<<< syscall.h >>>>>>

typedef int mapid_t;

struct mmap_container
{
  struct file *f;
  mapid_t mapid;
  void *uaddr;
  uint32_t offset_within_file;
  uint32_t size_used_within_page;
  struct list_elem elem
};

struct file_container 
{
  ...
  bool is_mmap;
};



<<<<<< thread.h >>>>>>
struct process {
  ...
  struct list mmap_containers; /* List of mmap containers storing informaiton about the pages of the file 
                                stored in physical memory by this process*/ 
}

```

## ALGORITHMS  

> C2: (3 marks)
> Explain how you determine whether a new file mapping overlaps with
> any existing segment and how you handle such a case. 
> Additionally, how might this interact with stack growth?

## RATIONALE  

> C3: (1 mark)
> Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables. How does your codebase take
> advantage of this?
