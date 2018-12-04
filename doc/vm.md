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

##  PAGE TABLE/FRAME MANAGEMENT

### DATA STRUCTURES  

> A1: (2 marks)
> Copy here the declaration of each new or changed 'struct' or
> 'struct' member, global or static variable, 'typedef', or
> enumeration that relates to your supplemental page table and
> frame table. Identify the purpose of each in roughly 25 words.

```
<<<<<< frame.h >>>>>>
   
[1]  #define MAX_FRAMES 1 << 16
    
[2]  struct frame
     {
[3]    uint32_t *vaddr;
[4]    struct list user_ptes;
[5]    struct list_elem elem;
[6]    bool pin;
     };
     
[7]  struct user_pte_ptr
     {
[8]    uint32_t *pagedir;
[9]    uint32_t *uaddr;
[10]   struct list_elem elem;
     };
    
[11] struct list frame_table;
```

`[1]`  Defining a constant to represent the maximum number of frames.  
`[2]`  A struct used for storing information about individual frames.  
`[3]`  The virtual address the frame manages.  
`[4]`  List that stores the PTEs pointing to this frame  
`[5]`  Struct list_elem used to add the struct to the list of frames in frame_table.  
`[6]`  Boolean used for pinning while the frame is swapped.  
`[7]`  A struct that acts as a pointer to a user page table entry.  
`[8]`  The pagedir of the user; used to find the PTE  
`[9]`  Index in the page directory/ page table; used to find the PTE  
`[10]` Struct list_elem used to add the struct to the list of user_ptes in struct frame  
`[11]` List that holds the frames, our frame table.  

### ALGORITHMS  

> A2: (2 marks) 
> Describe your code for finding the frame (if any) or other location that 
> contains the data of a given page.

> A3: (2 marks)
> How have you implemented sharing of read only pages?

### SYNCHRONIZATION  

> A4: (2 marks) 
> When two user processes both need a new frame at the same time,
> how are races avoided? You should consider both when there are
> and are not free frames available in memory.

### RATIONALE  

> A5: (2 marks)
> Why did you choose the data structure(s) that you did for
> representing the supplemental page table and frame table?




##  PAGING TO AND FROM DISK

### DATA STRUCTURES  

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
[1]   struct file *executable;
    };
    
    
<<<<<< swap.h >>>>>>
    
[2] struct bitmap *swap_table;
    
    
<<<<<< pte.h >>>>>>
    
[3] #define PTE_S 0x100 
    
    
<<<<<< exception.h >>>>>>
    
[4] #define PF_S 0x100 
[5] #define PF_F 0x200 
```

`[1]` Pointer to the executable file used for lazy loading.  
`[2]` A bitmap that records which sectors are currently storing swapped data.   
`[3]` Bit that is set to 1 when the page table entry is swapped, and 0 when it's not in swap.  
`[4]` Page fault error code bit that is 0 when it's not in swap table, and 1 when 
it is in swap table.  
`[5]` Page fault error code bit that is 0 when it's not a file, and 1 when it is a file.  

### ALGORITHMS  

> B2: (2 marks)
> When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

> B3: (2 marks)
> When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page directory (and any other 
> data structures) to reflect the frame Q no longer has?

### SYNCHRONIZATION  

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

### RATIONALE  

> B8: (2 marks)
> There is an obvious trade-off between parallelism and the complexity
> of your synchronisation methods. Explain where your design falls
> along this continuum and why you chose to design it this way.




##  MEMORY MAPPED FILES

### DATA STRUCTURES  

> C1: (1 mark)
> Copy here the declaration of each new or changed 'struct' or
> 'struct' member, global or static variable, 'typedef', or
> enumeration that relates to your file mapping table.  
> Identify the purpose of each in roughly 25 words.


```
<<<<<< syscall.h >>>>>>

[1] typedef int mapid_t;
    
[2] struct mmap_container
    {
[3]   struct file *f;
[4]   mapid_t mapid;
[5]   void *uaddr;
[6]   uint32_t offset_within_file;
[7]   uint32_t size_used_within_page;
[8]   struct list_elem elem;
    };
    
    struct file_container 
    {
      ...
[9]    bool is_mmap;
    };
    
    
<<<<<< thread.h >>>>>>

    struct process 
    {
      ...
[10]  struct list mmap_containers;  
    };
```

`[1]`  The map identifier.  
`[2]`  A struct used for storing information about the container for a memory mapped page of a file. 
`[3]`  A pointer to the file that is memory mapped.  
`[4]`  The unique identifier of the container.   
`[5]`  The address in user space that the page of the file is mapped to.   
`[6]`  The offset within the file where this page starts.  
`[7]`  How much of the page should be read (always less than or equal to PGSIZE).  
`[8]`  Struct list_elem used to add the struct to the list of mmap_containers in struct process.  
`[9]`  Boolean that acts as a flag and is true if that file is mapped to memory.  
`[10]` List of mmap containers storing informaiton about the pages of the file 
stored in physical memory by this process.   

### ALGORITHMS  

> C2: (3 marks)
> Explain how you determine whether a new file mapping overlaps with
> any existing segment and how you handle such a case. 
> Additionally, how might this interact with stack growth?

### RATIONALE  

> C3: (1 mark)
> Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables. How does your codebase take
> advantage of this?
