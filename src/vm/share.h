#ifndef SHARE_TABLE_H
#define SHARE_TABLE_H

#include <stdint.h>
#include <list.h>
#include "threads/synch.h"

struct shared_exec       /* Struct that stores information required for sharing */
{                        /* pages of read only executables between multiple processes */
  void *kvaddr;          /* Address in kernel virtual memory where frame holding 
                            page exists */
  char *name;            /* pointer to the thread name (that is also the executable name) */
  uint32_t start_read;   /* Offset in file to start reading */
  uint32_t read_bytes;   /* How much to read in file */
  struct list_elem elem; /*  */
};

struct list shared_execs;

#endif
