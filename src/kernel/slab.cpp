#include "slab.h"
#include "../drivers/serial.h"
#include "../include/string.h"
#include "heap.h"


#define PAGE_SIZE 4096

struct slab_header_t;

struct slab_cache_t {
  uint32_t object_size;
  slab_header_t *first_slab;
};

struct slab_header_t {
  uint32_t magic;        // Magic to identify slab pages
  slab_cache_t *cache;   // Pointer back to cache manager
  slab_header_t *next;   // Next slab in this cache
  slab_header_t *prev;   // Prev slab
  uint32_t free_objects; // Count of free objects
  uint32_t first_free;   // Index of first free object (freelist head)
                         // Objects follow immediately after header
  // Helper array (bitmask or linked list indices) might be needed,
  // but for simplicity we'll use an embedded free list in the objects
  // themselves.
};

// Caches for powers of 2 (32 to 2048)
// Indices:
// 0: 32
// 1: 64
// 2: 128
// 3: 256
// 4: 512
// 5: 1024
// 6: 2048
#define MAX_SLAB_INDEX 6
slab_cache_t slab_caches[MAX_SLAB_INDEX + 1];

// Helpers
int get_slab_index(uint32_t size) {
  if (size <= 32)
    return 0;
  if (size <= 64)
    return 1;
  if (size <= 128)
    return 2;
  if (size <= 256)
    return 3;
  if (size <= 512)
    return 4;
  if (size <= 1024)
    return 5;
  if (size <= 2048)
    return 6;
  return -1;
}

void slab_init() {
  uint32_t sizes[] = {32, 64, 128, 256, 512, 1024, 2048};
  for (int i = 0; i <= MAX_SLAB_INDEX; i++) {
    slab_caches[i].object_size = sizes[i];
    slab_caches[i].first_slab = 0;
  }
  serial_log("SLAB: Initialized.");
}

void *slab_alloc(uint32_t size) {
  int idx = get_slab_index(size);
  if (idx == -1)
    return 0; // Too big for slab

  slab_cache_t *cache = &slab_caches[idx];

  // 1. Search existing slabs
  slab_header_t *slab = cache->first_slab;
  while (slab) {
    if (slab->free_objects > 0) {
      break;
    }
    slab = slab->next;
  }

  // 2. Create new slab if needed
  if (!slab) {
    // Allocate a page from the MAIN heap (page aligned)
    // We use kmalloc_a(4096, 1, 0) but we need to ensure we don't recurse
    // infinitely. kmalloc calls slab_alloc. So we must ensure slab_alloc is NOT
    // called for 4096 bytes. 4096 > 2048, so it goes to main heap. Safe.
    uint32_t phys;
    void *page = kmalloc_real(PAGE_SIZE, 1, &phys);
    if (!page) {
      serial_log("SLAB: OOM allocating slab page!");
      return 0;
    }

    slab = (slab_header_t *)page;
    slab->magic = SLAB_MAGIC;
    slab->cache = cache;
    slab->prev = 0;
    slab->next = cache->first_slab;
    if (cache->first_slab) {
      cache->first_slab->prev = slab;
    }
    cache->first_slab = slab;

    // Initialize Objects
    // Layout: [Header] [Obj 0] [Obj 1] ...
    // Embed free list in objects. First 4 bytes of free object = index of next
    // free object.

    uint32_t available_size = PAGE_SIZE - sizeof(slab_header_t);
    uint32_t max_objs = available_size / cache->object_size;

    slab->free_objects = max_objs;
    slab->first_free = 0;

    uint8_t *base = (uint8_t *)slab + sizeof(slab_header_t);

    // Link all objects
    for (uint32_t i = 0; i < max_objs - 1; i++) {
      uint32_t *obj_ptr = (uint32_t *)(base + i * cache->object_size);
      *obj_ptr = i + 1; // Point to next index
    }
    // Last object
    uint32_t *last_obj =
        (uint32_t *)(base + (max_objs - 1) * cache->object_size);
    *last_obj = -1; // End of list
  }

  // 3. Allocate object
  uint32_t obj_idx = slab->first_free;
  uint8_t *base = (uint8_t *)slab + sizeof(slab_header_t);
  uint32_t *obj_ptr = (uint32_t *)(base + obj_idx * cache->object_size);

  slab->first_free = *obj_ptr; // Next free is what was stored here
  slab->free_objects--;

  // Clear the memory (good practice, also clears the freelist pointer)
  memset(obj_ptr, 0, cache->object_size);

  return (void *)obj_ptr;
}

int slab_free(void *ptr) {
  if (!ptr)
    return 0;

  // Check if this pointer belongs to a slab page
  // Slab pages are 4KB aligned, but kmalloc returns aligned+4.
  uint32_t page_start = (uint32_t)ptr & 0xFFFFF000;
  slab_header_t *slab = (slab_header_t *)(page_start + 4);

  if (slab->magic != SLAB_MAGIC) {
    return 0; // Not a slab page
  }

  // Calculate index
  uint8_t *base = (uint8_t *)slab + sizeof(slab_header_t);
  uint32_t offset = (uint32_t)ptr - (uint32_t)base;

  // Basic sanity check
  if (offset % slab->cache->object_size != 0) {
    serial_log("SLAB: Warning, freeing invalid pointer (misaligned)!");
    return 0; // Should panic/assert here
  }

  uint32_t obj_idx = offset / slab->cache->object_size;

  // Return to freelist
  uint32_t *obj_ptr = (uint32_t *)ptr;
  *obj_ptr = slab->first_free;
  slab->first_free = obj_idx;
  slab->free_objects++;

  // Optional: If slab is empty (all objects free), return page to kheap
  // But be careful about thrashing. For now, keep it simple.
  // If we wanted to free: must unlink from cache list first.

  return 1; // Handled
}
