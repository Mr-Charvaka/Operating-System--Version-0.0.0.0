#ifndef SLAB_H
#define SLAB_H

#include "../include/types.h"

// Slab Allocator for small sizes
// Supported sizes: 32, 64, 128, 256, 512, 1024, 2048
// Objects larger than 2048 go to the main Large Heap

#define SLAB_MAGIC 0x51AB51AB // "SLAB SLAB"

void slab_init();
void *slab_alloc(uint32_t size);
int slab_free(void *ptr); // Returns 1 if handled, 0 if not (passed to kfree)

#endif
