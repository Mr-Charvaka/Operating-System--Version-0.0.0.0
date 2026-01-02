#include <stdint.h>

void print(const char *str) { asm volatile("int $0x80" : : "a"(0), "b"(str)); }

void itoa(int n, char *s) {
  int i = 0, sign;
  if ((sign = n) < 0)
    n = -n;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  for (int j = 0, k = i - 1; j < k; j++, k--) {
    char temp = s[j];
    s[j] = s[k];
    s[k] = temp;
  }
}

void exit(int status) { asm volatile("int $0x80" : : "a"(12), "b"(status)); }

void _start() {
  uint32_t total, free, block_size;
  asm volatile("int $0x80"
               :
               : "a"(22), "b"((uintptr_t)&total), "c"((uintptr_t)&free),
                 "d"((uintptr_t)&block_size));

  uint32_t total_kb = (total * block_size) / 1024;
  uint32_t free_kb = (free * block_size) / 1024;
  uint32_t used_kb = total_kb - free_kb;

  char buf[32];
  print("\nDisk Usage (FAT16):\n");
  print("Total: ");
  itoa(total_kb, buf);
  print(buf);
  print(" KB\n");

  print("Used:  ");
  itoa(used_kb, buf);
  print(buf);
  print(" KB\n");

  print("Free:  ");
  itoa(free_kb, buf);
  print(buf);
  print(" KB\n");

  // Visual Bar
  print("[");
  int used_pct = (used_kb * 20) / total_kb;
  for (int i = 0; i < 20; i++) {
    if (i < used_pct)
      print("#");
    else
      print("-");
  }
  print("]\n");

  exit(0);
}
