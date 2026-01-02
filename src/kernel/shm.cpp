// Shared Memory - Implementation
#include "shm.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "heap.h"
#include "memory.h"
#include "pmm.h"
#include "process.h"
#include "vm.h"

extern "C" {

static shm_segment_t shm_segments[SHM_MAX_SEGMENTS];

void shm_init() {
  for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
    shm_segments[i].in_use = 0;
    shm_segments[i].ref_count = 0;
  }
  serial_log("SHM: Initialized.");
}

int sys_shmget(uint32_t key, uint32_t size, int flags) {
  (void)flags; // Simplified: ignore flags for now

  // Check if segment with this key already exists
  for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
    if (shm_segments[i].in_use && shm_segments[i].key == key) {
      return i; // Return existing segment ID
    }
  }

  // Find free slot
  int slot = -1;
  for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
    if (!shm_segments[i].in_use) {
      slot = i;
      break;
    }
  }

  if (slot < 0)
    return -1; // No free slots

  // Round up size to page boundary
  uint32_t pages = (size + SHM_PAGE_SIZE - 1) / SHM_PAGE_SIZE;
  if (pages == 0)
    pages = 1;
  uint32_t alloc_size = pages * SHM_PAGE_SIZE;

  // Allocate first physical page
  void *phys = pmm_alloc_block();
  if (!phys)
    return -1;

  // For multi-page allocations, we need contiguous pages
  // Simplified: just use the first page and note size
  // A real implementation would handle this better

  // Initialize segment
  shm_segments[slot].key = key;
  shm_segments[slot].size = alloc_size;
  shm_segments[slot].phys_addr = phys;
  shm_segments[slot].ref_count = 0;
  shm_segments[slot].in_use = 1;

  serial_log_hex("SHM: Created segment ", slot);
  return slot;
}

void *sys_shmat(int shmid) {
  if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS)
    return 0;
  if (!shm_segments[shmid].in_use)
    return 0;

  shm_segment_t *seg = &shm_segments[shmid];

  // Find a free virtual address range in user space
  // Use a simple bump allocator starting at 0x70000000
  static uint32_t next_shm_addr = 0x70000000;
  uint32_t virt = next_shm_addr;
  next_shm_addr += seg->size;

  // Map physical pages to virtual address
  uint32_t phys = (uint32_t)(uintptr_t)seg->phys_addr;
  uint32_t num_pages = seg->size / SHM_PAGE_SIZE;

  for (uint32_t i = 0; i < num_pages; i++) {
    vm_map_page(phys + i * SHM_PAGE_SIZE, virt + i * SHM_PAGE_SIZE,
                7); // User|RW|Present
  }

  seg->ref_count++;

  serial_log_hex("SHM: Attached segment at ", virt);
  return (void *)(uintptr_t)virt;
}

int sys_shmdt(void *addr) {
  // Find segment by address (simplified - just decrement ref count)
  // In a real implementation, we'd track per-process attachments

  (void)addr; // Simplified for now

  // Note: A full implementation would:
  // 1. Find which segment this address belongs to
  // 2. Unmap the pages from the process
  // 3. Decrement ref_count
  // 4. Free segment if ref_count == 0

  return 0;
}

} // extern "C"
