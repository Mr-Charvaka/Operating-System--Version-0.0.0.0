#include "pmm.h"
#include "../drivers/serial.h"
#include "../include/string.h"

// 32 blocks per uint32_t
#define BLOCKS_PER_UINT32 32

static uint32_t *pmm_bitmap = 0;
static uint32_t pmm_mem_size = 0;
static uint32_t pmm_max_blocks = 0;
static uint32_t pmm_used_blocks = 0;

// Set bit in map
static inline void mmap_set(int bit) {
  pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

// Unset bit in map
static inline void mmap_unset(int bit) {
  pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// Test bit in map
static inline int mmap_test(int bit) {
  return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

// Find first free block
// Returns -1 if no free blocks
static int mmap_first_free() {
  for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
    if (pmm_bitmap[i] != 0xFFFFFFFF) {
      for (int j = 0; j < 32; j++) {
        int bit = 1 << j;
        if (!(pmm_bitmap[i] & bit))
          return i * 32 + j;
      }
    }
  }
  return -1;
}

void pmm_init(uint32_t mem_size, uint32_t *bitmap) {
  pmm_mem_size = mem_size;
  pmm_bitmap = bitmap;
  pmm_max_blocks = (mem_size / PMM_BLOCK_SIZE);
  pmm_used_blocks =
      pmm_max_blocks; // Initially mark all as used, then free specific regions

  // By default, assume all memory is "used" or "reserved" until we explicitly
  // free available regions But for simplicity in this OS, we initially memset
  // to 0 (all free) then mark kernel used. However, safest involves calculating
  // size.

  // Size of bitmap in bytes = max_blocks / 8
  uint32_t bitmap_size = pmm_max_blocks / 8;
  memset((uint8_t *)pmm_bitmap, 0, bitmap_size); // Clear bitmap (0 = free)
  pmm_used_blocks = 0;

  serial_log("PMM: Initialized Bitmap.");
  serial_log_hex("  Mem Size:  ", mem_size);
  serial_log_hex("  Blocks:    ", pmm_max_blocks);
  serial_log_hex("  Bitmap at: ", (uint32_t)bitmap);
}

void pmm_mark_region_used(uint32_t base, uint32_t size) {
  uint32_t align = base / PMM_BLOCK_SIZE;
  uint32_t blocks = size / PMM_BLOCK_SIZE;
  if (size % PMM_BLOCK_SIZE)
    blocks++;

  for (; blocks > 0; blocks--) {
    if (!mmap_test(align)) {
      mmap_set(align);
      pmm_used_blocks++;
    }
    align++;
  }
}

void pmm_mark_region_free(uint32_t base, uint32_t size) {
  uint32_t align = base / PMM_BLOCK_SIZE;
  uint32_t blocks = size / PMM_BLOCK_SIZE;
  if (size % PMM_BLOCK_SIZE)
    blocks++;

  for (; blocks > 0; blocks--) {
    if (mmap_test(align)) {
      mmap_unset(align);
      pmm_used_blocks--;
    }
    align++;
  }
}

void *pmm_alloc_block() {
  if (pmm_get_free_block_count() <= 0) {
    serial_log("PMM: Out of Memory!");
    return 0;
  }

  int frame = mmap_first_free();
  if (frame == -1) {
    serial_log("PMM: Out of Memory (Verify)!");
    return 0;
  }

  mmap_set(frame);
  pmm_used_blocks++;

  uint32_t addr = frame * PMM_BLOCK_SIZE;
  return (void *)addr;
}

void pmm_free_block(void *p) {
  uint32_t addr = (uint32_t)p;
  int frame = addr / PMM_BLOCK_SIZE;

  if (mmap_test(frame)) {
    mmap_unset(frame);
    pmm_used_blocks--;
  }
}

uint32_t pmm_get_free_block_count() { return pmm_max_blocks - pmm_used_blocks; }

uint32_t pmm_get_block_count() { return pmm_max_blocks; }

void pmm_print_stats() {
  uint32_t free_blocks = pmm_get_free_block_count();
  uint32_t used_blocks = pmm_used_blocks;
  uint32_t total_blocks = pmm_max_blocks;

  serial_log("PMM Stats:");
  serial_log_hex("  Total Blocks: ", total_blocks);
  serial_log_hex("  Used Blocks:  ", used_blocks);
  serial_log_hex("  Free Blocks:  ", free_blocks);
  serial_log_hex("  Free Memory (KB): ", free_blocks * 4);
}
