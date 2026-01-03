/*
 * userlib.h - Utility Library for Retro-OS User Applications
 *
 * Provides common string functions, number conversion, and I/O helpers.
 */
#ifndef _USERLIB_H
#define _USERLIB_H

#include "syscall.h"

/* ============== STRING FUNCTIONS ============== */

static inline uint32_t strlen(const char *s) {
  uint32_t len = 0;
  while (s[len])
    len++;
  return len;
}

static inline int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline int strncmp(const char *s1, const char *s2, uint32_t n) {
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*dest++ = *src++))
    ;
  return d;
}

static inline char *strncpy(char *dest, const char *src, uint32_t n) {
  char *d = dest;
  while (n-- && (*dest++ = *src++))
    ;
  while (n--)
    *dest++ = 0;
  return d;
}

static inline char *strcat(char *dest, const char *src) {
  char *d = dest;
  while (*dest)
    dest++;
  while ((*dest++ = *src++))
    ;
  return d;
}

static inline char *strchr(const char *s, int c) {
  while (*s) {
    if (*s == c)
      return (char *)s;
    s++;
  }
  return c == 0 ? (char *)s : 0;
}

static inline char *strrchr(const char *s, int c) {
  const char *last = 0;
  while (*s) {
    if (*s == c)
      last = s;
    s++;
  }
  return c == 0 ? (char *)s : (char *)last;
}

static inline char *strstr(const char *haystack, const char *needle) {
  uint32_t nlen = strlen(needle);
  if (nlen == 0)
    return (char *)haystack;
  while (*haystack) {
    if (strncmp(haystack, needle, nlen) == 0)
      return (char *)haystack;
    haystack++;
  }
  return 0;
}

static inline void *memcpy(void *dest, const void *src, uint32_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  while (n--)
    *d++ = *s++;
  return dest;
}

static inline void *memmove(void *dest, const void *src, uint32_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dest;
}

static inline void *memset(void *s, int c, uint32_t n) {
  uint8_t *p = (uint8_t *)s;
  while (n--)
    *p++ = (uint8_t)c;
  return s;
}

static inline int memcmp(const void *s1, const void *s2, uint32_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  while (n--) {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++;
    p2++;
  }
  return 0;
}

static inline int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

static inline int isdigit(int c) { return c >= '0' && c <= '9'; }

static inline int isalpha(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int isalnum(int c) { return isalpha(c) || isdigit(c); }

static inline int isupper(int c) { return c >= 'A' && c <= 'Z'; }

static inline int islower(int c) { return c >= 'a' && c <= 'z'; }

static inline int toupper(int c) { return islower(c) ? c - 32 : c; }

static inline int tolower(int c) { return isupper(c) ? c + 32 : c; }

static inline int isprint(int c) { return c >= 0x20 && c < 0x7F; }

static inline int isxdigit(int c) {
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* ============== NUMBER CONVERSION ============== */

static inline int atoi(const char *s) {
  int result = 0;
  int sign = 1;
  while (isspace(*s))
    s++;
  if (*s == '-') {
    sign = -1;
    s++;
  } else if (*s == '+')
    s++;
  while (isdigit(*s)) {
    result = result * 10 + (*s - '0');
    s++;
  }
  return sign * result;
}

static inline long atol(const char *s) {
  long result = 0;
  int sign = 1;
  while (isspace(*s))
    s++;
  if (*s == '-') {
    sign = -1;
    s++;
  } else if (*s == '+')
    s++;
  while (isdigit(*s)) {
    result = result * 10 + (*s - '0');
    s++;
  }
  return sign * result;
}

static inline char *itoa(int value, char *str, int base) {
  char *p = str;
  char *p1, *p2;
  unsigned int uvalue = (unsigned int)value;

  if (base == 10 && value < 0) {
    *p++ = '-';
    uvalue = (unsigned int)(-value);
  }

  p1 = p;
  do {
    int digit = uvalue % base;
    *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
    uvalue /= base;
  } while (uvalue);

  *p = 0;
  p2 = p - 1;
  while (p1 < p2) {
    char tmp = *p1;
    *p1++ = *p2;
    *p2-- = tmp;
  }
  return str;
}

static inline char *utoa(unsigned int value, char *str, int base) {
  char *p = str;
  char *p1, *p2;

  p1 = p;
  do {
    int digit = value % base;
    *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
    value /= base;
  } while (value);

  *p = 0;
  p2 = p - 1;
  while (p1 < p2) {
    char tmp = *p1;
    *p1++ = *p2;
    *p2-- = tmp;
  }
  return str;
}

/* Format a number with leading zeros */
static inline void format_num(char *buf, int value, int width) {
  for (int i = width - 1; i >= 0; i--) {
    buf[i] = '0' + (value % 10);
    value /= 10;
  }
  buf[width] = 0;
}

/* ============== I/O HELPERS ============== */

/* Print a character */
static inline void putchar(char c) {
  char buf[2] = {c, 0};
  syscall_print(buf);
}

/* Print a string with newline */
static inline void puts(const char *s) {
  syscall_print(s);
  syscall_print("\n");
}

/* Print integer */
static inline void print_int(int value) {
  char buf[16];
  itoa(value, buf, 10);
  syscall_print(buf);
}

/* Print unsigned integer */
static inline void print_uint(unsigned int value) {
  char buf[16];
  utoa(value, buf, 10);
  syscall_print(buf);
}

/* Print hex */
static inline void print_hex(unsigned int value) {
  char buf[16];
  utoa(value, buf, 16);
  syscall_print("0x");
  syscall_print(buf);
}

/* Simple printf-like function */
static inline void printf(const char *fmt, ...) {
  /* Note: This is a simplified printf without full varargs support */
  /* For the OS, we use the direct print functions instead */
  syscall_print(fmt);
}

/* Read a line from stdin (fd 0) */
static inline int getline(char *buf, int maxlen) {
  int len = syscall_read(0, buf, maxlen - 1);
  if (len < 0)
    return len;
  if (len > 0 && buf[len - 1] == '\n')
    buf[--len] = 0;
  else
    buf[len] = 0;
  return len;
}

/* ============== FILE HELPERS ============== */

/* Get file size */
static inline int get_file_size(const char *path) {
  struct stat st;
  if (syscall_stat(path, &st) == 0)
    return st.st_size;
  return -1;
}

/* Check if path is a directory */
static inline int is_directory(const char *path) {
  struct stat st;
  if (syscall_stat(path, &st) == 0)
    return (st.st_mode & S_IFMT) == S_IFDIR;
  return 0;
}

/* Check if file exists */
static inline int file_exists(const char *path) {
  return syscall_access(path, 0) == 0;
}

/* ============== PARSING HELPERS ============== */

/* Skip whitespace */
static inline const char *skip_whitespace(const char *s) {
  while (*s && isspace(*s))
    s++;
  return s;
}

/* Parse next word, return pointer after word */
static inline const char *parse_word(const char *s, char *word, int maxlen) {
  s = skip_whitespace(s);
  int i = 0;
  while (*s && !isspace(*s) && i < maxlen - 1) {
    word[i++] = *s++;
  }
  word[i] = 0;
  return s;
}

/* Count words in string */
static inline int count_words(const char *s) {
  int count = 0;
  int in_word = 0;
  while (*s) {
    if (isspace(*s)) {
      in_word = 0;
    } else if (!in_word) {
      in_word = 1;
      count++;
    }
    s++;
  }
  return count;
}

/* ============== TIME/DATE HELPERS ============== */

static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed",
                                  "Thu", "Fri", "Sat"};
static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* Days since year 0 (simplified) */
static inline int days_in_month(int month, int year) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    return 29;
  return days[month - 1];
}

/* Calculate day of week (0 = Sunday) - Zeller's formula */
static inline int day_of_week(int year, int month, int day) {
  if (month < 3) {
    month += 12;
    year--;
  }
  int k = year % 100;
  int j = year / 100;
  int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  return ((h + 6) % 7);
}

/* ============== RANDOM NUMBER ============== */

static unsigned int rand_seed = 12345;

static inline void srand(unsigned int seed) { rand_seed = seed; }

static inline unsigned int rand(void) {
  rand_seed = rand_seed * 1103515245 + 12345;
  return (rand_seed >> 16) & 0x7FFF;
}

/* ============== SORTING ============== */

/* Simple bubble sort for strings */
static inline void sort_strings(char **arr, int n) {
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (strcmp(arr[j], arr[j + 1]) > 0) {
        char *tmp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = tmp;
      }
    }
  }
}

/* ============== CHECKSUMS ============== */

/* Simple checksum */
static inline uint32_t checksum(const uint8_t *data, uint32_t len) {
  uint32_t sum = 0;
  while (len--)
    sum += *data++;
  return sum;
}

/* CRC32 lookup table */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c2, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7a9f, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

static inline uint32_t crc32(const uint8_t *data, uint32_t len) {
  uint32_t crc = 0xFFFFFFFF;
  while (len--) {
    crc = crc32_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

#endif /* _USERLIB_H */
