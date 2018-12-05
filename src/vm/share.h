#ifndef SHARE_TABLE_H
#define SHARE_TABLE_H

#include <stdint.h>
#include <list.h>
#include "threads/synch.h"

struct shared_exec
{
  void *kaddr;
  char *name;
  uint32_t start_read;
  uint32_t read_bytes;
  struct lock lock;
  struct list_elem elem;
};

struct list shared_execs;

#endif
