// ============================================================================
// mprotect.cpp - Memory Protection Implementation
// Entirely hand-crafted for Retro-OS kernel
// ============================================================================

#include "../drivers/serial.h"
#include "../include/errno.h"
#include "heap.h"
#include "memory.h"
#include "paging.h" // Correct local include
#include "process.h"
#include "vm.h"


extern "C" {

// ============================================================================
// Memory Protection Constants
// ============================================================================
#define PROT_NONE 0x0  // Page cannot be accessed
#define PROT_READ 0x1  // Page can be read
#define PROT_WRITE 0x2 // Page can be written
#define PROT_EXEC 0x4  // Page can be executed

// msync flags
#define MS_ASYNC 1      // Perform asynchronous writes
#define MS_SYNC 2       // Perform synchronous writes
#define MS_INVALIDATE 4 // Invalidate cached pages

// madvise flags
#define MADV_NORMAL 0     // No special treatment
#define MADV_RANDOM 1     // Expect random page references
#define MADV_SEQUENTIAL 2 // Expect sequential page references
#define MADV_WILLNEED 3   // Will need these pages
#define MADV_DONTNEED 4   // Don't need these pages
#define MADV_FREE 8       // Free pages only if memory pressure

// mlock limits
#define MCL_CURRENT 1 // Lock all currently mapped pages
#define MCL_FUTURE 2  // Lock all pages mapped in the future
#define MCL_ONFAULT 4 // Lock after page fault

// ============================================================================
// mprotect - Set memory protection
// ============================================================================
int sys_mprotect(void *addr, size_t len, int prot) {
  if (!addr)
    return -EINVAL;

  // Address must be page-aligned
  uintptr_t start = (uintptr_t)addr;
  if (start & 0xFFF)
    return -EINVAL;

  // Length must be non-zero
  if (len == 0)
    return 0;

  // Round up length to page boundary
  size_t pages = (len + 0xFFF) / 0x1000;

  // Convert PROT_* to page flags
  uint32_t flags = PTE_PRESENT;
  if (prot & PROT_WRITE)
    flags |= PTE_WRITE;
  if (!(prot & PROT_EXEC))
    flags |= PTE_NX; // No-execute if execute not requested

  // Apply protection to each page
  for (size_t i = 0; i < pages; i++) {
    uintptr_t vaddr = start + i * 0x1000;

    // Get current page table entry
    uint32_t *pte = paging_get_pte(vaddr);
    if (!pte || !(*pte & PTE_PRESENT)) {
      // Page not mapped - skip
      continue;
    }

    // Preserve physical address, update flags
    uint32_t phys = *pte & ~0xFFF;
    *pte = phys | flags;
  }

  // Flush TLB for modified range
  for (size_t i = 0; i < pages; i++) {
    uintptr_t vaddr = start + i * 0x1000;
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
  }

  serial_log("MPROTECT: Changed protection for pages");
  return 0;
}

// ============================================================================
// msync - Synchronize mapped memory
// ============================================================================
int sys_msync(void *addr, size_t length, int flags) {
  if (!addr)
    return -EINVAL;

  // Address must be page-aligned
  uintptr_t start = (uintptr_t)addr;
  if (start & 0xFFF)
    return -EINVAL;

  (void)length;
  (void)flags;

  // For now, memory is not actually file-backed, so sync is a no-op
  return 0;
}

// ============================================================================
// mlock/munlock - Lock/unlock memory pages
// ============================================================================
int sys_mlock(const void *addr, size_t len) {
  if (!addr)
    return -EINVAL;

  // Address must be page-aligned
  uintptr_t start = (uintptr_t)addr;
  if (start & 0xFFF)
    return -EINVAL;

  (void)len;

  // Memory locking not fully implemented (all pages are "locked" in RAM)
  return 0;
}

int sys_munlock(const void *addr, size_t len) {
  if (!addr)
    return -EINVAL;

  uintptr_t start = (uintptr_t)addr;
  if (start & 0xFFF)
    return -EINVAL;

  (void)len;

  // Memory unlocking not fully implemented
  return 0;
}

int sys_mlockall(int flags) {
  (void)flags;
  // Lock all pages - simplified
  return 0;
}

int sys_munlockall(void) {
  // Unlock all pages - simplified
  return 0;
}

// ============================================================================
// madvise - Memory advice
// ============================================================================
int sys_madvise(void *addr, size_t length, int advice) {
  if (!addr)
    return -EINVAL;

  (void)length;
  (void)advice;

  // Memory advice is hints only - just accept and ignore
  return 0;
}

int sys_posix_madvise(void *addr, size_t len, int advice) {
  return sys_madvise(addr, len, advice);
}

// ============================================================================
// posix_memalign - Aligned memory allocation
// ============================================================================
int sys_posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (!memptr)
    return EINVAL;

  // Alignment must be power of 2 and multiple of sizeof(void*)
  if (alignment < sizeof(void *))
    return EINVAL;
  if (alignment & (alignment - 1))
    return EINVAL; // Not power of 2

  if (size == 0) {
    *memptr = 0;
    return 0;
  }

  // Allocate with extra space for alignment
  size_t total = size + alignment - 1 + sizeof(void *);
  void *raw = kmalloc(total);
  if (!raw)
    return ENOMEM;

  // Align the result
  uintptr_t aligned =
      ((uintptr_t)raw + sizeof(void *) + alignment - 1) & ~(alignment - 1);

  // Store original pointer before aligned address
  ((void **)aligned)[-1] = raw;

  *memptr = (void *)aligned;
  return 0;
}

// ============================================================================
// aligned_alloc - C11 aligned allocation
// ============================================================================
void *aligned_alloc(size_t alignment, size_t size) {
  void *ptr = 0;
  if (sys_posix_memalign(&ptr, alignment, size) != 0)
    return 0;
  return ptr;
}

} // extern "C"
