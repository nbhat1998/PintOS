<h1 align="center" style="white-space:pre">
OS 211 
TASK 2: USER PROGRAMS 
DESIGN DOCUMENT 
</h1>

# Pintos Group 13

- George Soteriou <gs2617@ic.ac.uk>
- Niranjan Bhat <nb1317@ic.ac.uk>
- Maria Teguiani <mt5217@ic.ac.uk>
- Iulia Ivana <imi17@ic.ac.uk>

## ARGUMENT PASSING

---

### DATA STRUCTURES

> A1: (1 mark)
>
> Copy here the declaration of each new or changed 'struct' or 'struct' member, global or static variable, 'typedef', or enumeration. Identify the purpose of each in roughly 25 words.

We didn't add any of the above in order to implement argument passing.

### ALGORITHMS

> A2: (2 marks)
>
> How does your argument parsing code avoid overflowing the stack page?
> What are the efficiency considerations of your approach?

Before decreasing the stack pointer and writing to the stack page, we
check that it doesn't point below PHYS_BASE - PGSIZE.
The run-time is O(n \* argc), where argc is constant and n is the most significant
factor, so it is actually runs in linear time.

### RATIONALE

> A3: (3 marks)
>
> Why does Pintos implement strtok_r() but not strtok()?

strtok_r() is the reentrant version of strtok(), which stores the state between calls,
and therefore allows it to be called from multiple threads at once. Doing the same
with strtok() would lead to undefined behaviour.

> A4: (2 marks)
>
> In Pintos, the kernel separates commands into a executable name and arguments.
> In Unix-like systems, the shell does this separation.
> Identify two advantages of the Unix approach.

1. It prevents malicious code injection into the kernel space.
2. ? ? ? ? ? same but accidental? ? ?

## SYSTEM CALLS

---

### DATA STRUCTURES

> B1: (6 marks)
>
> Copy here the declaration of each new or changed 'struct' or 'struct' member, global or static variable, 'typedef', or enumeration. Identify the purpose of each in roughly 25 words.

```
<<<<<< thread.h >>>>>>

  struct thread
  {
     ...
[1]  struct list child_processes;
[2]  struct process *process;
     ...
  };

[3]struct process
  {
[4]  tid_t pid;
[5]  struct semaphore sema;
[6]  struct semaphore setup_sema;
[7]  struct lock lock;
[8]  int status;
[9]  struct list_elem elem;
[10] bool first_done;
[11] bool setup;
[12] struct list file_containers;
  };
```

`[1]` A list of child processes (struct process), which stores all the processes started by this thread.  
 `[2]` Current thread's own struct process.

`[3]` Struct that stores the necessary members for representing and synchronizing a process.  
 `[4]` The process ID, which is the same as the tid.  
 `[5]` A semaphore used when a parent thread waits for a child thread.  
 `[6]` A semaphore used in sys_exec() to notify the parent that the child has finished loading, regardless of the outcome of load().  
 `[7]` A lock used to synchronize operations on struct process.  
 `[8]` The curret status of the process.  
 `[9]` Struct list_elem used to add struct process to the parent thread's list of child threads.  
 `[10]` A boolean that is set to true when either the parent thread or the child thread finishes execution.  
 `[11]` A boolean that is set to true if the process loaded successfully, and false otherwise.  
 `[12]` A list of struct file_container, used for storing information about all the files opened by this process.

```
<<<<<< syscall.h >>>>>>

[1]struct file_container {
[2]  int fd;
[3]  struct file *f;
[4]  struct list_elem elem;
  };

[5]  struct lock filesys_lock;

[6]  #define WORD_LENGTH 4
[7]  #define BYTE 8
```

`[1]` A struct used for storing information about files.  
`[2]` A unique file descriptor.  
`[3]` A pointer to the actual file.  
`[4]` Struct list_elem used to add the struct to the list of file containers in struct process.  
`[5]` A lock used to lock the file system when editing it.  
`[6]` Defining a constant to represent the length of a word in bytes.  
`[7]` Defining a constant to represent the length of a byte in bits.

### ALGORITHMS

> B2: (2 marks)
>
> Describe how your code ensures safe memory access of user provided data from within the kernel.

We use the get_user() and put_user() functions when writing and reading to and from kernel memory and check wether they are successful before actually accessing the data.

> B3: (3 marks)
>
> Suppose that we choose to verify user provided pointers by validating them before use (i.e. using the first method described in the spec).
> What is the least and the greatest possible number of inspections of the page table (e.g. calls to pagedir_get_page()) that would need to be made in the following cases?
>
> a) A system call that passes the kernel a pointer to 10 bytes of user data.  
> b) A system call that passes the kernel a pointer to a full page (4,096 bytes) of user data.  
> c) A system call that passes the kernel a pointer to 4 full pages (16,384 bytes) of user data.
>
> You should briefly explain your checking tactic and how it applies to each case to generate your answers.

a) Min: 1, Max: 4 

  Case Minimum : If 10 bytes of data are present within the page, for successful verification, 2 inspections would be required. In case of unsuccessful verification, only 1 inspection would be required as after the first inspection fails, further checking would not proceed.

  Case Maximum: If 10 bytes of data are split over 2 pages of data, for successful verification, 4 checks would be required - 2 checks for the boundaries of the 10 bytes of data, 1 check for the lower bound of the upper page the data is present in and 1 check for the upper bound of the lower page the data is present in. 





b) Min: 1, Max: 4  


c) Min: 1, Max: 10

> B4: (2 marks)
>
> How do you keep the purpose of your user memory verification code clear and avoid obscuring the primary function of code in a morass of error-handling?
> Additionally, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?

We implemented the functions get_word() and check_ptr() which provide abstraction from memory verification when writing the primary function. get_word() actually returns 4 bytes, which is the standard size of an argument. check_ptr() checks if the all the characters in the char\* provided by the user are valid.  
Before returning from any function, we free every resource allocated in that function.

> B5: (8 marks)
>
> Describe your implementation of the "wait" system call and how it interacts with process termination for both the parent and child.

First, we iterate through the list of child processes to find the process with the corresponding pid (if the child process if not found in the list, we return -1). We check if the child process is not done, and if so, we sema_down to wait for it to terminate (the child process will call sema_up upon termination, regardless of how it terminated). Otherwise, we get the status of the child process, remove it from the list, free it, and finally return the status. If wait is called again on the same child, it will no longer be in the list, and the function will return -1.

### SYNCHRONIZATION

> B6: (2 marks)
>
> The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading. How does your code ensure this? How is the load success/failure status passed back to the thread that calls "exec"?

After calling process_execute(), the parent thread calls sema_down on the child process' setup_sema. It then waits until the child process finishes loading and once it does, it stores the loading status in the setup member of the child process, and then calls sema_up. This informs the parent wether the child laoded successfully or not, and returns the tid of the child process, or -1 respectively.

> B7: (5 marks)
>
> Consider parent process P with child process C.  
> How do you ensure proper synchronization and avoid race conditions when:
>
> i) P calls wait(C) before C exits?  
> ii) P calls wait(C) after C exits?  
> iii) P terminates, without waiting, before C exits?  
> iv) P terminates, without waiting, after C exits?
>
> Additionally, how do you ensure that all resources are freed regardless of the above case?

i) P will look through its list of children process structs, lock each child and check if that child has the tid we are looking for. When it finds C, it will check and see that C has not exited, then release the lock, and sema_down() to wait until C is done. Regardless of how C exits, it will always call sema_up() after storing its exit status. P will then reacquire the lock and store the status of C. Then P removes C from the list, frees C and returns the status.

ii) P will look through its list of children process structs, lock each child and check if that child has the tid we are looking for. When it finds C, it will then check and see that C has exited, it will then store the status of C, remove C from the list, free C and return the status.

iii) When P terminates, it will iterate through all its children, locking at each child, until it finds C. As for all other children, in the case that C has not exited, it will set a flag showing that the parent has exited, and release the lock.

iv) When P terminates, it will iterate through all its children, locking at each child, until it finds C. As for all other children, in the case that C has exited, it will remove C from the list, and free C.

### RATIONALE

> B8: (2 marks)
>
> Why did you choose to implement safe access of user memory from the kernel in the way that you did?

Our implementation uses the helper functions get_word() and check_ptr . This provides abstration between the primary function and safe access of memory, it makes our code clearer,  
and we also get rid of code duplication.

> B9: (2 marks)
>
> What advantages or disadvantages can you see to your design for file
> descriptors?

There will be an xcessive number of file containers eventually formed with many of them pointing to the same file.
Our implementation also requires iterating over the entire list of file descriptors to look for a particular fd, which is not particularily efficient.
An advanage of our design is that it is easy to understand and implement.
