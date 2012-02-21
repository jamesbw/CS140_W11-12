#ifndef FRAME_H
#define FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <hash.h>

struct frame {
  struct hash_elem elem;
  void *upage;
  void *kpage;
  void *addr;
  uint32_t *pd;
};


#endif
