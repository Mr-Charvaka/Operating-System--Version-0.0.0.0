#ifndef HEAP_H
#define HEAP_H

#include "buddy.h"

// Define a header structure for our heap blocks
typedef struct header {
  struct header *next; // Next block
  struct header *prev; // Previous block
  uint32_t size;       // Size of data (including header)
  uint8_t allocated;   // 1 if allocated, 0 if free
  uint32_t magic;      // verification bytes (Canary)
} header_t;

// Define a heap structure
typedef struct {
  uint32_t start_address;
  uint32_t end_address;
  uint32_t max_address;
  uint8_t supervisor;
  uint8_t readonly;
  header_t *first_block;
  buddy_t buddy;
} kheap_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the kernel heap
// generic implementation of malloc/free
void init_kheap(uint32_t start, uint32_t end, uint32_t max);

void *kmalloc_real(uint32_t size, int align, uint32_t *phys);
void kfree(void *p);

// Wrappers
void *malloc(uint32_t size);
void free(void *p);

#ifdef __cplusplus
}
#endif

#endif
