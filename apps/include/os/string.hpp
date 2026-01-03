#pragma once

#include <stddef.h>
#include <stdint.h>

namespace OS {
namespace String {
static inline size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

static inline void strcpy(char *dest, const char *src) {
  while (*src) {
    *dest++ = *src++;
  }
  *dest = 0;
}

static inline void memcpy(void *dest, const void *src, size_t n) {
  char *d = (char *)dest;
  const char *s = (const char *)src;
  while (n--) {
    *d++ = *s++;
  }
}

static inline void memset(void *dest, int val, size_t n) {
  unsigned char *ptr = (unsigned char *)dest;
  while (n-- > 0)
    *ptr++ = val;
}
} // namespace String
} // namespace OS
