#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1   /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2   /* 0: read, 1: write. */
#define PF_U 0x4   /* 0: kernel, 1: user process. */
#define PF_S 0x100 /* 0: not in swap table, 1: in swap table */
#define PF_F 0x200 /* 0: not an executable, 1: executable */
#define PF_M 0x500 /* 0: not a mmap file, 1: mmap file */

void exception_init(void);
void exception_print_stats(void);

#endif /* userprog/exception.h */
