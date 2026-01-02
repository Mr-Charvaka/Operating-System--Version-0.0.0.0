#include "../include/types.h"

// Define size_t
typedef uint32_t size_t;

extern "C" {
void *kmalloc(uint32_t size);
void serial_log(const char *msg);
}

void *operator new(size_t size) {
  void *ptr = kmalloc(size);
  // DEBUG: Log allocation
  // serial_log_hex("NEW: Alloc size ", size);
  // serial_log_hex("NEW: Alloc ptr  ", (uint32_t)ptr);
  if (!ptr) {
    serial_log("NEW: FAILED ALLOCATION!");
  }
  return ptr;
}

void *operator new[](size_t size) { return kmalloc(size); }

void operator delete(void *ptr) {
  // kfree(ptr);
}

void operator delete[](void *ptr) {
  // kfree(ptr);
}

void operator delete(void *ptr, size_t size) {
  (void)size;
  // kfree(ptr);
}

extern "C" void __cxa_pure_virtual() {
  serial_log("[C++] Panic: pure virtual function called!");
  // Infinite loop or proper panic
  while (1) {
  }
}
