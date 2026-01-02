extern "C" {
#include "../include/string.h"

void *memcpy(void *dest, const void *src, int count) {
  // Optimization: Copy 32-bit words if aligned
  if (count >= 4 && ((uint32_t)dest & 3) == 0 && ((uint32_t)src & 3) == 0) {
    uint32_t *d32 = (uint32_t *)dest;
    const uint32_t *s32 = (const uint32_t *)src;
    while (count >= 4) {
      *d32++ = *s32++;
      count -= 4;
    }
    dest = (void *)d32;
    src = (const void *)s32;
  }

  // Copy remaining bytes
  char *dest8 = (char *)dest;
  const char *src8 = (const char *)src;
  while (count--) {
    *dest8++ = *src8++;
  }
  return dest;
}

void *memset(void *dest, char val, int count) {
  char *dest8 = (char *)dest;
  while (count--) {
    *dest8++ = val;
  }
  return dest;
}

int strlen(const char *str) {
  int len = 0;
  while (str[len])
    len++;
  return len;
}

// Simple itoa implementation
void reverse(char arr[], int start, int end) {
  while (start < end) {
    char temp = arr[start];
    arr[start] = arr[end];
    arr[end] = temp;
    start++;
    end--;
  }
}

char *itoa(int num, char *str, int base) {
  int i = 0;
  int isNegative = 0;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // In standard itoa(), negative numbers are handled only with
  // base 10. Otherwise numbers are considered unsigned.
  if (num < 0 && base == 10) {
    isNegative = 1;
    num = -num;
  }

  // Process individual digits
  while (num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if (isNegative)
    str[i++] = '-';

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  reverse(str, 0, i - 1);

  return str;
}

char *strcpy(char *dest, const char *src) {
  char *temp = dest;
  while ((*dest++ = *src++))
    ;
  return temp;
}

int memcmp(const void *s1, const void *s2, int n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  while (n--) {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++;
    p2++;
  }
  return 0;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcat(char *dest, const char *src) {
  char *ptr = dest + strlen(dest);
  while (*src)
    *ptr++ = *src++;
  *ptr = 0;
  return dest;
}

int strncmp(const char *s1, const char *s2, int n) {
  while (n > 0 && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
}
