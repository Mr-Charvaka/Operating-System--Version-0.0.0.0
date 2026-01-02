#include "heap.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "memory.h"
#include "paging.h" // For getting physical address if needed
#include "slab.h"

kheap_t kheap;
int slab_is_initialized = 0;

extern uint32_t *kernel_directory;

// Helper to expand heap
void expand_unix_heap(uint32_t new_size) {
  // For now, we assume fixed size heap at init.
  // Dynamic expansion requires page allocation which we'll add later.
  serial_log("HEAP: Use fixed size for now.");
}

void init_kheap(uint32_t start, uint32_t end, uint32_t max) {
  if (start % 4096 != 0 || end % 4096 != 0) {
    serial_log("HEAP: Addresses must be page aligned!");
    return;
  }

  kheap.start_address = start;
  kheap.end_address = end;
  kheap.max_address = max;
  kheap.supervisor = 1;
  kheap.readonly = 0;

  // Create the first huge hole
  header_t *hole = (header_t *)start;
  hole->size = end - start - sizeof(header_t);
  hole->allocated = 0;
  hole->magic = 0xAB;
  hole->next = 0;
  hole->prev = 0;

  kheap.first_block = hole;
  serial_log("HEAP: Initialized Linked List Allocator.");
  serial_log_hex("  Start: ", start);
  serial_log_hex("  End:   ", end);
  serial_log_hex("  Size:  ", hole->size);
}

#include "../include/io.h"

void *kmalloc_real(uint32_t size, int align, uint32_t *phys) {
  cli();

  // Try Slab Allocator first (if initialized, size small, and NOT aligned)
  // Slab aligns to object size usually. 4096 aligned requests must bypass slab.
  if (slab_is_initialized && size <= 2048 && !align) {
    void *ptr = slab_alloc(size);
    if (ptr) {
      if (phys) {
        // Slab objects are in mapped kernel memory.
        // For simplicity assume Identity mapped or calculate.
        // Since our kernel heap is Identity mapped (init_paging 0-272MB),
        // Virtual == Physical.
        *phys = (uint32_t)ptr;
      }
      sti();
      return ptr;
    }
  }

  if (size == 0) {
    sti();
    return 0;
  }

  // Align size to 4 bytes
  if (size & 3) {
    size = (size & 0xFFFFFFFC) + 4;
  }

  // Simple page alignment request not fully supported inside a block yet
  // unless we split. For generic kmalloc, we ignore 'align' unless specific.
  // If align=1 (page aligned), we need to find a block that happens to be page
  // aligned or padding it. This implementation is basic.

  header_t *cur = kheap.first_block;
  while (cur) {
    if (!cur->allocated) {
      uint32_t data_start = (uint32_t)cur + sizeof(header_t);
      uint32_t adjusted_start = data_start;
      if (align && (adjusted_start & 0xFFF)) {
        adjusted_start = (adjusted_start + 0xFFF) & 0xFFFFF000;
      }

      // Always reserve 4 bytes before adjusted_start to store the header
      // pointer for kfree to work with both aligned and unaligned blocks.
      adjusted_start += 4; // reserve space for header pointer

      uint32_t required_size = size + (adjusted_start - data_start);

      if (cur->size >= required_size) {
        // Found a block!
        // Split if too big
        if (cur->size > required_size + sizeof(header_t) + 32) {
          header_t *split = (header_t *)(adjusted_start + size);
          uint32_t split_data_size = cur->size - (adjusted_start - data_start) -
                                     size - sizeof(header_t);

          split->size = split_data_size;
          split->allocated = 0;
          split->magic = 0xAB;
          split->next = cur->next;
          split->prev = cur;
          if (cur->next)
            cur->next->prev = split;
          cur->next = split;

          cur->size = (adjusted_start - data_start) + size;
        }

        cur->allocated = 1;
        cur->magic = 0xCD;

        void *ptr = (void *)adjusted_start;
        // Store actual header pointer just before the returned buffer
        ((uint32_t *)ptr)[-1] = (uint32_t)cur;

        memset(ptr, 0, size);
        if (phys)
          *phys = (uint32_t)ptr;

        sti();
        return ptr;
      }
    }
    cur = cur->next;
  }

  serial_log("HEAP: OOM! Requested:");
  serial_log_hex("Size: ", size);
  header_t *dbg = kheap.first_block;
  int count = 0;
  while (dbg && count < 30) {
    serial_log_hex("Block Size: ", dbg->size);
    serial_log_hex("  Allocated: ", dbg->allocated);
    serial_log_hex("  Magic: ", dbg->magic);
    serial_log_hex("  Addr: ", (uint32_t)dbg);
    dbg = dbg->next;
    count++;
  }
  serial_log("HEAP: OOM! No block large enough.");
  sti();
  return 0; // OOM
}

void kfree(void *p) {
  cli();
  if (p == 0) {
    sti();
    return;
  }

  // Try Slab Free
  if (slab_is_initialized && slab_free(p)) {
    sti();
    return;
  }

  // Get the actual header from the stored pointer
  uint32_t header_ptr = ((uint32_t *)p)[-1];
  if (header_ptr < kheap.start_address || header_ptr > kheap.end_address) {
    // This is suspicious. If slab didn't handle it, it might be corrupted.
    serial_log("HEAP: Invalid header pointer - potential corruption!");
    serial_log_hex("  Pointer: ", (uint32_t)p);
    serial_log_hex("  Header:  ", header_ptr);
    sti();
    return;
  }
  header_t *header = (header_t *)header_ptr;

  if (header->magic != 0xCD) {
    serial_log("HEAP: Double free or corruption!");
    serial_log_hex("  Pointer: ", (uint32_t)p);
    serial_log_hex("  Magic:   ", (uint32_t)header->magic);
    sti();
    return;
  }

  header->allocated = 0;
  header->magic = 0xAB;

  // Coalesce right
  if (header->next && !header->next->allocated) {
    header->size += header->next->size + sizeof(header_t);
    header->next = header->next->next;
    if (header->next)
      header->next->prev = header;
  }

  // Coalesce left
  if (header->prev && !header->prev->allocated) {
    header->prev->size += header->size + sizeof(header_t);
    header->prev->next = header->next;
    if (header->next)
      header->next->prev = header->prev;
  }
  sti();
}

void *malloc(uint32_t size) { return kmalloc_real(size, 0, 0); }

void free(void *p) { kfree(p); }
