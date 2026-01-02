#ifndef MEMORY_H
#define MEMORY_H

#include "../include/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_memory(uint32_t start_address);
void *kmalloc(uint32_t size);
void *kmalloc_a(uint32_t size, int align, uint32_t *phys);
void kmalloc_align_page();
void set_heap_status(int status);

#ifdef __cplusplus
}
#endif

#endif
