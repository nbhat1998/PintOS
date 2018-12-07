<h1 align="center" style="white-space:pre">
OS 211 
TASK 3: VIRTUAL MEMORY
DESIGN DOCUMENT
</h1>

# Pintos Group 13

- George Soteriou <gs2617@ic.ac.uk>
- Niranjan Bhat <nb1317@ic.ac.uk>
- Iulia Ivana <imi17@ic.ac.uk>
- Maria Teguiani <mt5217@ic.ac.uk>

## PAGE TABLE/FRAME MANAGEMENT

### DATA STRUCTURES

> A1: (2 marks)
> Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration that relates to your supplemental page table and frame table. Identify the purpose of each in roughly 25 words.

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


```

`[1]` A struct used for storing information about individual frames.  
`[2]` The kernel virtual address the frame manages.  
`[3]` List that stores the page table entries pointing to the kernel virtual address pointed to
by this frame.  
`[4]` Struct list_elem used to add the struct to the list of frames in frame_table.  
`[5]` Boolean used for pinning while the frame is swapped.  
`[6]` A lock used to synchronize operations on struct frame.  
`[7]` A struct that acts as a pointer to a user page table entry.  
`[8]` The pagedir of the user; used to find the page table entry.  
`[9]` Index in the page directory/ page table; used to find the PTE  
`[10]` Struct list_elem used to add the struct to the list of user_ptes in struct frame  
`[11]` List that holds the frames (our frame table).

### ALGORITHMS

> A2: (2 marks)
> Describe your code for finding the frame (if any) or other location that contains the data of a given page.

The page table entry is used to keep track of the frame as the Address part of it points to a frame directly. The frame table has a list of frames that can be uniquely identified using the address stored in the pte, and also contain a list of `struct user_pte_ptr`s that can be used to refer back to the page table entry itself.  
In the case that the frame is swapped out, the page table entry holds the index in swap where the page can be found and a bit to show that it is swapped. This is used to restore the frame.

> A3: (2 marks)
> How have you implemented sharing of read only pages?

During page faults, in the case that we are handling a read_only page which is being lazy loaded, we look through the list of shared execs.  
If we find a shared_exec struct (shown in section C) that corresponds to the file we want to load at the correct offset, we just link the new `uaddr` to that frame and return.
Otherwise, a new `shared_exec` struct is created and stores information about where to read from in the file. Then it is actually read into memory and the frame where is is loaded, is stored in the struct.
The information stored is needed in the case of evicting, because if a lazy loaded executable is to be evicted, it is just cleared and information about how to load it back is stored in all the page table entries that require it.

### SYNCHRONIZATION

> A4: (2 marks)
> When two user processes both need a new frame at the same time, how are races avoided? You should consider both when there are and are not free frames available in memory.

If there are free frames in memory there is no problem as `palloc_get_page` with return a new address of a free frame every time.
In the case that the memory is full, two evictions must take place. Since we lock around choosing a frame to evict, the choice will happen sequentially but after we select a frame to evict, we 'pin' that frame using the pin bool, so that we do not allow another process to evict it too.

### RATIONALE

> A5: (2 marks)
> Why did you choose the data structure(s) that you did for representing the supplemental page table and frame table?

**Supplemental page table:**  
We chose not to use a supplemental page table as we found that the existing page table was sufficient enough to implement the required functionalities. We only used the remaining free bits of the flags in ptes to distinguish between lazy loaded files, swapped frames, mmaped files and stack growth pages.
This means that we had to limit the total amount of swapped pages to 2^20, which we believe is sufficient for the use case.
The address part of the pts is used in many different ways if the pte is not present:

- For lazy loading, it stores a combination of file offset and size to read.
- For swapping, it stores the index in swap where the page can be found
- For mmaped files and stack growth it stores nothing

**Frame table:**  
The frame table is a list of currently allocated frames. We decided to use this implementation as it is easy to use. The frame table has a list of page table entries that point to the frame. It is also easy to check if this list is empty when a process dies, and if so, free the frame.

<br>

## PAGING TO AND FROM DISK

### DATA STRUCTURES

> B1: (1 mark)
> Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration that relates to your swap table. Identify the purpose of each in roughly 25 words.

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
[7] #define PF_M 0x500

<<<<<< frame.h >>>>>>

[7] struct list_elem* evict_ptr;
```

`[1]` User stack pointer to the current end of the stack that can be accessed from an
interrupt context, used when `f->esp` is the kernel stack pointer and we need the user one.  
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
> When a frame is required but none is free, some frame must be evicted. Describe your code for choosing a frame to evict.

Our eviction algorithm uses the Clock Replacement Policy, also known as the Second Chance Replacement Policy. A brief explanation of the Cock Replacement Policy:

The candidate pages for removal are considered in a round robin order. Pages that have been accessed between consecutive calls to the eviction policy have no chance of being replaced, and will not be considered.
When considered in a round robin order, the page that will be replaced is one of the pages that has not been accessed since the last call to the eviction policy.
Each memory frame has a "second chance" bit, which is set to 1 every time it is referenced.  
Each new page being read into a memory frame has its second chance set to 0.
When a page needs to removed (evicted), the memory frames are traversed in a round robin order, and the following actions are carried out:s

1. If the second chance bit of the memory frame being considered is the second chance bit is reset to 0, and the next frame is considered. A frame which has the second chance bit set to 1 will not be evicted.
2. If the second chance bit of the memory frame being considered is 0, the page in that memory frame is selected for eviction.

In our particular use case, for evicting frame table entries from the frame table, we used the second chance replacement policy in the following manner:

1. `list_elem *evict_ptr` is used to keep cyclically iterate over the frame table in a round robin order.
2. Each frame's second choice is checked by iterating over the frame's list of user_ptes and checking the accessed bit of the page directories to see if they have been accessed since the last eviction. If so, they are set to 0 and the frame is not selected for eviction.
3. When a user virtual address is accessed anywhere throughout the program, we make sure to set the accessed bit for that corresponding page table entry by using the pagedir_set_accessed function.
4. A data member bool pin is added to each frame and is used to synchronize eviction in case that another process is already trying to evict that frame.

> B3: (2 marks)
> When a process P obtains a frame that was previously used by a process Q, how do you adjust the page directory (and any other data structures) to reflect the frame Q no longer has?

The page table entry in the page directory is zeroed out and therefore does not point to the kernel virtual address that is used to identify the previous frame. The frame in the frame table also removes its entry of the pointer to the uaddr of Q and repaces it with the uaddr of P.

### SYNCHRONIZATION

> B4: (2 marks)
> Explain how your synchronization design prevents deadlock. (You may want to refer to the necessary conditions for deadlock.)

We use fine grained locking in our frame table. While traversing the list we lock each frame as we are looking through it. This prevents simultaneous modifications to the same frame. in the cases where both the frame lock and the filesystem lock is required, we always acquire the frame lock first, to prevent deadlocks.

> B5: (2 marks)
> A page fault in process P can cause another process Q's frame to be evicted. How do you ensure that Q cannot access or modify the page during the eviction process?

Once a frame is chosen for eviction, we first set all the ptes to no longer point to the frame, and then zero out the page so Q cannot modify the new page as it has no pointer to it.

> B6: (2 marks)
> A page fault in process P can cause another process Q's frame to be evicted. How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

Since we pin pages during eviction, when Q tries to fault the page back in, it will not choose the same frame as before to fault the page back in.

> B7: (2 marks)
> Explain how you handle access to paged-out user pages that occur during system calls.

If a `fault_addr` was pointing to a frame that has now been swapped out, it will now have a swap bit and the index in swap where the page can be found, stored in its pte. We detect the swap bit, and call swap read, a function that will get a new frame (by evicting another one if required) and then swapping the page back in to the new frame. It will then set up the pte of that `fault_addr` to point to the the new frame.

### RATIONALE

> B8: (2 marks)
> There is an obvious trade-off between parallelism and the complexity of your synchronisation methods. Explain where your design falls along this continuum and why you chose to design it this way.

We went for a less complex synchronisation method as we prevent deadlocks by obtaining resources in the same order every time, but fine grained locking on the frame table made for quite good parallelism.

## MEMORY MAPPED FILES

### DATA STRUCTURES

> C1: (1 mark)
> Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration that relates to your file mapping table. Identify the purpose of each in roughly 25 words.

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

<<<<<< share.h >>>>>>

[11] struct shared_exec
     {
[12]   void *kvaddr;
[13]   char *name;
[14]   uint32_t start_read;
[15]   uint32_t read_bytes;
[16]   struct list_elem elem;
     };

[17] struct list shared_execs;
```

`[1]` A typedef for the map identifier.  
`[2]` A struct used for storing information about the container for a memory mapped page of a file.  
`[3]` A pointer to the file that is memory mapped.  
`[4]` The unique identifier of the container.  
`[5]` The address in user space that the page of the file is mapped to.  
`[6]` The offset within the file where this page starts.  
`[7]` Size used within the page in user virtual space (always less than or equal to PGSIZE)  
`[8]` Struct list_elem used to add the struct to the list of mmap_containers in struct process.  
`[9]` Boolean that acts as a flag and is true if that file is mapped to memory. If true, when syscall file_close is called, we don't actually close the file.  
`[10]` List of mmap containers storing information about the pages of the file
stored in physical memory by this process.  
`[11]` A struct used for storing information required for sharing pages of read only executables between multiple processes.  
`[12]` The address in kernel virtual memory where frame holding page exists.  
`[13]` The pointer to the thread name, which is also the executable's name.  
`[14]` The offset in file where it needs to start reading.  
`[15]` The number of characters that must be read from that page in the file.  
`[16]` Struct list_elem used to add the struct to the list of shared execs.  
`[17]` List of struct shared_exec that keeps track of the shared executables.

### ALGORITHMS

> C2: (3 marks)
> Explain how you determine whether a new file mapping overlaps with any existing segment and how you handle such a case. Additionally, how might this interact with stack growth?

When a new file mapping is being created, the size of the file in pages (let this be x) to be mapped is retrieved, and we make sure that x pages, starting from the user virtual address passed are eligible to be written to. We check this by doing the following:
We iterate x times in the user virtual address space and check if each user virtual address has anything mapped to it, by looking through its page table entry and checking if it is either NULL or 0.
We use the function get_pte to do this check. By passing false to this function, we ensure that a new pte is not created on lookup.
If the pte has a mapping to an existing frame in memory, we return -1 to signal a failed attempt to map a file.
In case that the PTE is 0, the page fault handler will create a new page in stack as it is within range of the stack pointer. In case it is NULL, no pte will be created and the stack will not be affected.

### RATIONALE

> C3: (1 mark)
> Mappings created with "mmap" have similar semantics to those of data demand-paged from executables. How does your codebase take advantage of this?

During load of executables and mmapped files, ptes are created corresponding to pages in the file by storing the offset in the file, along with how much to read for that page.

In page faults we then load the required pages according to where the address faults.
Our codebase takes advantage of this by using the same page fault function used in lazy loading, albeit falling through into an entirely different if block with different conditions. This reduces a small amount of code duplication, as the data used within the function (such as fault address, not_present, bool write and bool user) are the same regardless of which if block within the page_fault function our code cascades through.
