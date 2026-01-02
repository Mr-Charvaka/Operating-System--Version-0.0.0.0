#include <stddef.h>
#include <stdint.h>

// Syscall wrapper
static inline int32_t syscall(int32_t id, int32_t a, int32_t b, int32_t c) {
  int32_t ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(id), "b"(a), "c"(b), "d"(c));
  return ret;
}

void print(const char *str) { syscall(0, (int32_t)str, 0, 0); }

void *sbrk(intptr_t increment) {
  return (void *)syscall(6, (int32_t)increment, 0, 0);
}

void *mmap(void *addr, uint32_t length, int prot, int flags, int fd,
           uint32_t offset) {
  return (void *)syscall(7, (int32_t)addr, (int32_t)length, prot); // Simplified
}

int munmap(void *addr, uint32_t length) {
  return syscall(8, (int32_t)addr, (int32_t)length, 0);
}

int fork() { return syscall(9, 0, 0, 0); }

static const char msg_hello[] = "Hello from User Space (Isolated)!\n";
static const char msg_testing[] = "Testing sbrk...\n";
static const char msg_failed[] = "sbrk failed!\n";
static const char msg_succeeded[] = "sbrk succeeded!\n";
static const char msg_mmap_test[] = "Testing mmap...\n";
static const char msg_mmap_ok[] = "mmap succeeded!\n";
static const char msg_munmap_test[] = "Testing munmap...\n";
static const char msg_munmap_ok[] = "munmap returned.\n";
static const char msg_verified[] = "Memory Write/Read Verified!\n";
static const char msg_fail_verify[] = "Memory Verification Failed!\n";
static const char msg_fork_test[] = "Testing fork...\n";
static const char msg_parent[] = "I am the parent!\n";
static const char msg_child[] = "I am the child!\n";
static const char msg_complete[] = "User process complete. Spinning...\n";

void _start() {
  print(msg_hello);

  // Test sbrk
  print(msg_testing);
  char *ptr = (char *)sbrk(4096);
  if ((int32_t)ptr == -1) {
    print(msg_failed);
  } else {
    print(msg_succeeded);
    ptr[0] = 'A';
    ptr[4095] = 'Z';
    if (ptr[0] == 'A' && ptr[4095] == 'Z') {
      print(msg_verified);
    } else {
      print(msg_fail_verify);
    }
  }

  // Test mmap
  print(msg_mmap_test);
  char *mptr = (char *)mmap(NULL, 1024, 0, 0, -1, 0);
  if ((uint32_t)mptr == 0) {
    print(msg_failed);
  } else {
    print(msg_mmap_ok);
    mptr[0] = 'M';
    mptr[1023] = 'P';
    if (mptr[0] == 'M' && mptr[1023] == 'P') {
      print(msg_verified);
    } else {
      print(msg_fail_verify);
    }

    // Test munmap
    print(msg_munmap_test);
    munmap(mptr, 1024);
    print(msg_munmap_ok);
  }

  // Test fork
  print(msg_fork_test);
  int pid = fork();
  if (pid < 0) {
    print(msg_failed);
  } else if (pid == 0) {
    print(msg_child);
  } else {
    print(msg_parent);
  }

  print(msg_complete);

  // Loop forever
  for (volatile int i = 0;; i++) {
    asm volatile("nop");
  }
}
