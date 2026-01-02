#include <stdint.h>

struct dirent {
  char name[128];
  uint32_t inode;
};

void print(const char *str) { asm volatile("int $0x80" : : "a"(0), "b"(str)); }

int open(const char *path, int flags) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(2), "b"(path), "c"(flags));
  return res;
}

int readdir(int fd, uint32_t index, struct dirent *de) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(21), "b"(fd), "c"(index), "d"(de));
  return res;
}

void close(int fd) { asm volatile("int $0x80" : : "a"(5), "b"(fd)); }

void exit(int status) { asm volatile("int $0x80" : : "a"(12), "b"(status)); }

void _start() {
  print("\n--- File Manager (Root) ---\n");
  int fd = open("/", 0);
  if (fd < 0) {
    print("Error opening root directory\n");
    exit(1);
  }

  struct dirent de;
  uint32_t i = 0;
  while (readdir(fd, i++, &de) == 0) {
    print("  ");
    print(de.name);
    print("\n");
  }

  close(fd);
  exit(0);
}
