#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

// 4KB Block Size
#define PMM_BLOCK_SIZE 4096
#define PMM_BLOCKS_PER_BYTE 8

// API to initialize the PMM
// mem_size: Total physical memory size in bytes
// bitmap: Pointer to a valid memory region to store the bitmap
void pmm_init(uint32_t mem_size, uint32_t *bitmap);

// Allocate a single 4KB block
// Returns physical address, or 0 if OOM
void *pmm_alloc_block();

// Allocate contiguous 4KB blocks
void *pmm_alloc_contiguous_blocks(uint32_t count);
void pmm_free_contiguous_blocks(void *p, uint32_t count);

// Free a 4KB block
void pmm_free_block(void *p);

// Helper to mark a specific region as used (e.g., Kernel code, Modules)
void pmm_mark_region_used(uint32_t base, uint32_t size);

// Helper to mark a specific region as free
void pmm_mark_region_free(uint32_t base, uint32_t size);

// Get total free blocks
uint32_t pmm_get_free_block_count();

uint32_t pmm_get_block_count();

// Print memory statistics
void pmm_print_stats();

#endif
