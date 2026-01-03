#pragma once

#include "../syscall.h" // Reuse existing C definitions for constants
#include <stdint.h>

namespace OS {

// Encapsulate syscalls in a namespace
namespace Syscall {

static inline void print(const char *str) {
  asm volatile("int $0x80" ::"a"(SYS_PRINT), "b"(str));
}

static inline int getpid() {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETPID));
  return res;
}

static inline int open(const char *path, int flags) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_OPEN), "b"(path), "c"(flags));
  return res;
}

static inline int read(int fd, void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(size));
  return res;
}

static inline int write(int fd, const void *buf, uint32_t size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(size));
  return res;
}

static inline int close(int fd) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_CLOSE), "b"(fd));
  return res;
}

static inline void *sbrk(intptr_t increment) {
  void *res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SBRK), "b"(increment));
  return res;
}

static inline int fork() {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_FORK));
  return res;
}

static inline int execve(const char *path, char **argv, char **envp) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_EXECVE), "b"(path), "c"(argv), "d"(envp));
  return res;
}

static inline void exit(int status) {
  asm volatile("int $0x80" : : "a"(SYS_EXIT), "b"(status));
  while (1)
    ;
}

static inline int wait(int *status) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_WAIT), "b"(status));
  return res;
}

static inline int socket(int domain, int type, int protocol) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SOCKET), "b"(domain), "c"(type), "d"(protocol));
  return res;
}

static inline int connect(int sockfd, const char *path) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_CONNECT), "b"(sockfd), "c"(path));
  return res;
}

static inline int shmget(uint32_t key, uint32_t size, int flags) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SHMGET), "b"(key), "c"(size), "d"(flags));
  return res;
}

static inline void *shmat(int shmid) {
  void *res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SHMAT), "b"(shmid));
  return res;
}

static inline int shmdt(void *addr) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SHMDT), "b"(addr));
  return res;
}

static inline int sleep(uint32_t ticks) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SLEEP), "b"(ticks));
  return res;
}

static inline int statfs(uint32_t *total, uint32_t *free,
                         uint32_t *block_size) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_STATFS), "b"(total), "c"(free), "d"(block_size));
  return res;
}

static inline int readdir(int fd, uint32_t index, struct dirent *de) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_READDIR), "b"(fd), "c"(index), "d"(de));
  return res;
}

static inline int unlink(const char *path) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_UNLINK), "b"(path));
  return res;
}

static inline int alarm(uint32_t seconds) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_ALARM), "b"(seconds));
  return res;
}

static inline int lseek(int fd, int offset, int whence) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SEEK), "b"(fd), "c"(offset), "d"(whence));
  return res;
}

static inline int getppid() {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_GETPPID));
  return res;
}

static inline int pipe(int *fds) {
  int res;
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_PIPE), "b"(fds));
  return res;
}

} // namespace Syscall
} // namespace OS
