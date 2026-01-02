#include "memory.h"
#include "../include/io.h"
#include "heap.h"

extern "C" {

static uint32_t free_mem_addr = 0;
int heap_ready = 0;

void init_memory(uint32_t start_address) { free_mem_addr = start_address; }

// Internal placement malloc (before Heap is up)
void *placement_kmalloc(uint32_t size, int align, uint32_t *phys) {
  if (align && (free_mem_addr & 0xFFFFF000)) {
    free_mem_addr &= 0xFFFFF000;
    free_mem_addr += 0x1000;
  }
  if (phys) {
    *phys = free_mem_addr;
  }
  uint32_t tmp = free_mem_addr;
  free_mem_addr += size;
  return (void *)tmp;
}

void *kmalloc(uint32_t size) {
  if (heap_ready)
    return malloc(size);
  return placement_kmalloc(size, 0, 0);
}

void *kmalloc_a(uint32_t size, int align, uint32_t *phys) {
  if (heap_ready)
    return kmalloc_real(size, align, phys);
  return placement_kmalloc(size, align, phys);
}

void kmalloc_align_page() {
  // Only meaningful for placement (skips to next page)
  if (free_mem_addr & 0xFFF) {
    free_mem_addr &= 0xFFFFF000;
    free_mem_addr += 0x1000;
  }
}

void set_heap_status(int status) { heap_ready = status; }

} // extern "C"
