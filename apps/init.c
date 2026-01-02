#include <stdint.h>

void print(const char *str) { asm volatile("int $0x80" : : "a"(0), "b"(str)); }

int fork() {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(9));
  return res;
}

int execve(const char *path, char **argv, char **envp) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(10), "b"(path), "c"(argv), "d"(envp));
  return res;
}

int wait(int *status) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(11), "b"(status));
  return res;
}

void exit(int status) { asm volatile("int $0x80" : : "a"(12), "b"(status)); }

int read(int fd, void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(3), "b"(fd), "c"(buf), "d"(size));
  return res;
}

int open(const char *path, int flags) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(2), "b"(path), "c"(flags));
  return res;
}

int close(int fd) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(5), "b"(fd));
  return res;
}

struct dirent {
  char name[128];
  uint32_t inode;
};

int readdir(int fd, uint32_t index, struct dirent *de) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(21), "b"(fd), "c"(index), "d"(de));
  return res;
}

int chdir(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(23), "b"(path));
  return res;
}

int unlink(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(24), "b"(path));
  return res;
}

int mkdir(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(25), "b"(path));
  return res;
}

int getcwd(char *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(26), "b"(buf), "c"(size));
  return res;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, uint32_t n) {
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void strcpy(char *dest, const char *src) {
  while ((*dest++ = *src++))
    ;
}

void _start() {
  print("\n--- Anti Gravity OS ---\n");
  print("Type HELP for a list of commands.\n");

  char cwd[128];

  while (1) {
    getcwd(cwd, 127);
    print("\n");
    print(cwd);
    print("> ");

    char line[128];
    int len = read(0, line, 127);
    if (len <= 0)
      continue;

    // Remove newline
    if (line[len - 1] == '\n')
      line[len - 1] = 0;
    else
      line[len] = 0;

    if (line[0] == 0)
      continue;

    // Direct command matching or simple parsing
    if (strcmp(line, "CLS") == 0 || strcmp(line, "cls") == 0) {
      print("\033[2J\033[H"); // ANSI clear screen
    } else if (strcmp(line, "VER") == 0 || strcmp(line, "ver") == 0) {
      print("Anti Gravity OS [Version 1.0.11]\n");
    } else if (strcmp(line, "HELP") == 0 || strcmp(line, "help") == 0) {
      print("Internal: DIR, TYPE, CLS, ECHO, VER, HELP, DEL, MD, CD, EXIT\n");
      print("Apps: calc, df, fm, hello\n");
    } else if (strcmp(line, "DIR") == 0 || strcmp(line, "dir") == 0) {
      int fd = open(".", 0);
      if (fd < 0)
        fd = open("/", 0);
      if (fd >= 0) {
        struct dirent de;
        int i = 0;
        while (readdir(fd, i++, &de) == 0) {
          print(de.name);
          print("\n");
        }
        close(fd);
      }
    } else if (strncmp(line, "CD ", 3) == 0 || strncmp(line, "cd ", 3) == 0) {
      if (chdir(line + 3) != 0)
        print("Invalid directory\n");
    } else if (strncmp(line, "MD ", 3) == 0 || strncmp(line, "md ", 3) == 0) {
      if (mkdir(line + 3) != 0)
        print("Error creating directory\n");
    } else if (strncmp(line, "DEL ", 4) == 0 || strncmp(line, "del ", 4) == 0) {
      if (unlink(line + 4) != 0)
        print("Error deleting file\n");
    } else if (strncmp(line, "ECHO ", 5) == 0 ||
               strncmp(line, "echo ", 5) == 0) {
      print(line + 5);
      print("\n");
    } else if (strncmp(line, "TYPE ", 5) == 0 ||
               strncmp(line, "type ", 5) == 0) {
      int fd = open(line + 5, 0);
      if (fd >= 0) {
        char file_buf[512];
        int flen;
        while ((flen = read(fd, file_buf, 511)) > 0) {
          file_buf[flen] = 0;
          print(file_buf);
        }
        close(fd);
        print("\n");
      } else {
        print("File not found\n");
      }
    } else if (strcmp(line, "EXIT") == 0 || strcmp(line, "exit") == 0) {
      exit(0);
    } else if (strcmp(line, "calc") == 0) {
      int pid = fork();
      if (pid == 0) {
        execve("CALC.ELF", 0, 0);
        exit(1);
      } else
        wait(0);
    } else if (strcmp(line, "df") == 0) {
      int pid = fork();
      if (pid == 0) {
        execve("DF.ELF", 0, 0);
        exit(1);
      } else
        wait(0);
    } else if (strcmp(line, "fm") == 0) {
      int pid = fork();
      if (pid == 0) {
        execve("FM.ELF", 0, 0);
        exit(1);
      } else
        wait(0);
    } else if (strcmp(line, "hello") == 0) {
      int pid = fork();
      if (pid == 0) {
        execve("HELLO.ELF", 0, 0);
        exit(1);
      } else
        wait(0);
    } else {
      print("Unknown command: ");
      print(line);
      print("\n");
    }
  }
}
