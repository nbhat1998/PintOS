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

[1]  struct frame
     {
[2]    uint32_t *kvaddr;
[3]    struct list user_ptes;
[4]    struct list_elem elem;
[5]    bool pin;
[6]    struct lock lock;
     };
     
[7]  struct user_pte_ptr
     {
[8]    uint32_t *pagedir;
[9]   uint32_t *uaddr;
[10]   struct list_elem elem;
     };
    
[11] struct list frame_table;


<<<<<< share.h >>>>>>

[12] struct shared_exec
     {
[13]   void *kvaddr;
[14]   char *name;
[15]   uint32_t start_read;
[16]   uint32_t read_bytes;
[17]   struct list_elem elem;
     };

[18] struct list shared_execs;
```

`[1]`  A struct used for storing information about individual frames.  
`[2]`  The kernel virtual address the frame manages.  
`[3]`  List that stores the page table entries pointing to the kernel virtual address pointed to
by this frame.  
`[4]`  Struct list_elem used to add the struct to the list of frames in frame_table.  
`[5]`  Boolean used for pinning while the frame is swapped.   
`[6]`  A lock used to synchronize operations on struct frame.  
`[7]`  A struct that acts as a pointer to a user page table entry.  
`[8]`  The pagedir of the user; used to find the page table entry.   
`[9]`  Index in the page directory/ page table; used to find the PTE  
`[10]` Struct list_elem used to add the struct to the list of user_ptes in struct frame  
`[11]` List that holds the frames (our frame table).  
`[12]` A struct used for storing information required for sharing pages of read only executables
between multiple processes.   
`[13]` The address in kernel virtual memory where frame holding page exists.   
`[14]` The pointer to the thread name, which is also the executable's name.  
`[15]` The offset in file where it needs to start reading.  
`[16]` The number of characters that must be read from that page in the file.  
`[17]` Struct list_elem used to add the struct to the list of shared execs.   
`[18]` List of struct shared_exec that keeps track of the shared executables.    

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
[1]   void *esp;
[2]   struct file *executable;
    };
    
    
<<<<<< swap.h >>>>>>
    
[3] struct bitmap *swap_table;
    
    
<<<<<< pte.h >>>>>>
    
[4] #define PTE_S 0x100 
    
    
<<<<<< exception.h >>>>>>
    
[5] #define PF_S 0x100 
[6] #define PF_F 0x200 
[7] #define PF_M 0x500 /* 0: not a mmap file, 1: mmap file */


<<<<<< frame.h >>>>>>

[7] struct list_elem* evict_ptr;
```

`[1]` User stack pointer to the current end of the stack that can be accessed from an
interrupt context, used when f->esp is the kernel stack pointer and we need the user one. 
`[2]` Pointer to the executable file used for lazy loading.  
`[3]` A bitmap that records which sectors are currently storing swapped data.   
`[4]` Bit that is set to 1 when the page table entry is swapped, and 0 when it's not in swap.  
`[5]` Page fault error code bit that is 0 when it's not in swap table, and 1 when it is.   
`[6]` Page fault error code bit that is 0 when it's not an executable, and 1 when it is.  
`[7]` Page fault error code bit that is 0 when it's not a memory mapped file, and 1 when it is.  
`[8]` The list_elem corresponding to the entry in the frame table which we will next check when 
calling evict().  

### ALGORITHMS  

> B2: (2 marks)
> When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

Our eviction algorithm uses the Clock Replacement Policy, also known as the Second Chance Replacement Policy. 
A brief explanation of the Cock Replacement Policy:  
    The candidate pages for removal are considered in a round robin order. Pages that have been accessed between consecutive calls to the eviction policy have no chance of being replaced, and will not be considered. 
    When considered in a round robin order, the page that will be replaced is one of the pages that has not been accessed since the last call to the eviction policy. 
    Each memory frame has a "second chance" bit, which is set to 1 everytime it is referenced. 
    Each new page being read into a memory frame has its second chance set to 0. 
    When a page needs to removed (evicted), the memory frames are traversed in a round robin order, and the following actions are carried out:  
        1. If the second chance bit of the memory frame being considered is 1, the second chance bit is reset to 0, and the next frame is considered. A frame which has the second chance bit set to 1 will not be evicted.  
        2. If the second chance bit of the memory frame being considered is 0, the page in that memory frame is selected for eviction.  


In our particular use case, for evicting frame table entries from the frame table, we modified and used the second chance replacement policy in the following manner:   
    1. A list_elem *evict_ptr is used to keep cyclically iterate over the frame table in a round robin order.   
    2. Each frame table entry's second choice is decided by iterating over the frame's list of user_ptes, which is the list of all user page table entries (which in turn are pointed to by a user virtual address) that    point to the kernel virtual address pointed to by the frame table entry, and checking their accessed bit to see if they have been accessed since the last function call to evict.   
    3. When a user virtual address is accessed anywhere throughout the program, we make sure to set the accessed bit for that corresponding page table entry by using the pagedir_set_accessed function.  
    4. A data member bool pin is added to each frame table entry and is used to synchronize eviction in case that two threads are trying to evict the same frame table entry.   



> B3: (2 marks)
> When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page directory (and any other 
> data structures) to reflect the frame Q no longer has?

The page table entry in the page directory is zeroed out and therefore does not point to the kernel 
virtual address that is used to identify the previous frame.


### SYNCHRONIZATION  

> B4: (2 marks)
> Explain how your synchronization design prevents deadlock.  
> (You may want to refer to the necessary conditions for deadlock.)

Mutual exclusion (each resource is either available or assigned to exactly one process)

Hold and wait (process can request resources while it holds other resources earlier)

No preemption (resources given to a process cannot be forcibly revoked)

Circular wait (two or more processes in a circular chain, each waiting for a resource held by the next process)


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

`[1]`  A typedef for the map identifier.  
`[2]`  A struct used for storing information about the container for a memory mapped page of a file.  
`[3]`  A pointer to the file that is memory mapped.  
`[4]`  The unique identifier of the container.   
`[5]`  The address in user space that the page of the file is mapped to.   
`[6]`  The offset within the file where this page starts.  
`[7]`  Size used within the page in user virtual space (always less than or equal to PGSIZE)  
`[8]`  Struct list_elem used to add the struct to the list of mmap_containers in struct process.  
`[9]`  Boolean that acts as a flag and is true if that file is mapped to memory.  
`[10]` List of mmap containers storing informaiton about the pages of the file 
stored in physical memory by this process.   

### ALGORITHMS  

> C2: (3 marks)
> Explain how you determine whether a new file mapping overlaps with
> any existing segment and how you handle such a case. 
> Additionally, how might this interact with stack growth?

When a new file mapping is being created, the size of the file (let this be x) to be mapped is retrieved, and we make sure that x pages, starting from the user virtual address passed are elibile to be written to. We check this by doing the following: 
    As the user virtual address to create a new mapping to changes by PGSIZE at every step, we retrieve the page table entry corresponding to the user virtual address by passing it to the get_pte(..., false) which is essentially just a wrapper around lookup_page(...) as lookup_page(...) is a static function which cannot be accessed outside of pagedir.c 
    Using false as the last parameter ensures that the a value is returned iff the page table entry exists. If no page table entry exists, a NULL value is returned.
    For every page table entry that falls in the range of (user virtual address) to (user virtual address + number of pages), if it has a non-NULL value and if the contents are non-zero, it means that there is some data already present at that page table entry, which means that overlapping will occur. In this case, we return -1, which crashes the program. It is not necessary to return -1 if no page table entry exists, beause if no page table entry exists, there will be no data, and overlapping is not a concern. 


### RATIONALE  

> C3: (1 mark)
> Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables. How does your codebase take
> advantage of this?

During lazy loading, a page fault occurs at the address where a page is not found, which then enters the exception page_fault and loads the page into the desired user virtual address. 

When an mmap mapping is created, pages from the file aren't actually loaded into the user virtual space specified by the mapping. When a process expects a page to exist at the user virtual address, it doesn't find the required page in the location which triggers a page fault at the current location. Our codebase takes advantage of this by using the same page fault function used in lazy loading, albeit falling through into an entirely different if block with different conditions. This reduces a small amount of code duplication, as the data used within the function (such as fault address, not_present, bool write and bool user) are the same regardless of which if block within the page_fault function our code cascades through.