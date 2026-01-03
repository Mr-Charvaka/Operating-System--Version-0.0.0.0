/*
 * posix_impl.cpp - Implementation of standard POSIX functions for user apps
 *
 * This file implements the standard POSIX functions declared in headers like
 * signal.h, time.h, etc. by calling the underlying system calls.
 */

#include "include/syscall.h"
#include "pthread.h"
#include "semaphore.h"
#include "signal.h"
#include "time.h"
#include "types.h"

extern "C" {

// ============================================================================
// Signal Functions
// ============================================================================

int kill(int pid, int signum) { return syscall_kill(pid, signum); }

unsigned int alarm(unsigned int seconds) { return syscall_alarm(seconds); }

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SIGACTION), "b"(signum), "c"(act), "d"(oldact));
  return res;
}

int sigemptyset(sigset_t *set) {
  if (!set)
    return -1;
  *set = 0;
  return 0;
}

// ============================================================================
// Time Functions
// ============================================================================

unsigned int sleep(unsigned int seconds) { return syscall_sleep(seconds); }

// ============================================================================
// Pthread Functions
// ============================================================================

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
  int res;
  // SYS_PTHREAD_CREATE = 96
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_PTHREAD_CREATE), "b"(thread), "c"(attr),
                 "d"(start_routine), "S"(arg));
  return res;
}

int pthread_join(pthread_t thread, void **retval) {
  int res;
  // SYS_PTHREAD_JOIN = 98
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_PTHREAD_JOIN), "b"(thread), "c"(retval));
  return res;
}

int pthread_detach(pthread_t thread) {
  int res;
  // SYS_PTHREAD_DETACH = 99
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_PTHREAD_DETACH), "b"(thread));
  return res;
}

void pthread_exit(void *retval) {
  // SYS_PTHREAD_EXIT = 97
  asm volatile("int $0x80" ::"a"(SYS_PTHREAD_EXIT), "b"(retval));
  while (1)
    ;
}

pthread_t pthread_self(void) { return (pthread_t)syscall_getpid(); }

// ============================================================================
// Semaphore Functions
// ============================================================================

int sem_init(sem_t *sem, int pshared, unsigned int value) {
  if (!sem)
    return -1;
  sem->value = (int)value;
  sem->waiters = 0;
  return 0;
}

int sem_destroy(sem_t *sem) { return 0; }

int sem_wait(sem_t *sem) {
  int res;
  // SYS_SEM_WAIT = 109
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SEM_WAIT), "b"(sem));
  return res;
}

int sem_post(sem_t *sem) {
  int res;
  // SYS_SEM_POST = 110
  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_SEM_POST), "b"(sem));
  return res;
}

// ============================================================================
// Process Management
// ============================================================================

int getpid() { return syscall_getpid(); }
int getppid() { return syscall_getppid(); }

int fork() { return syscall_fork(); }

int wait(int *status) { return syscall_wait(status); }

int uname(struct utsname *buf) { return syscall_uname(buf); }

// ============================================================================
// File Operations
// ============================================================================

int open(const char *path, int flags, ...) { return syscall_open(path, flags); }

int close(int fd) { return syscall_close(fd); }

ssize_t read(int fd, void *buf, size_t count) {
  return (ssize_t)syscall_read(fd, buf, (uint32_t)count);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return (ssize_t)syscall_write(fd, buf, (uint32_t)count);
}

off_t lseek(int fd, off_t offset, int whence) {
  return (off_t)syscall_lseek(fd, (int)offset, whence);
}

int stat(const char *path, struct stat *buf) { return syscall_stat(path, buf); }

int unlink(const char *path) { return syscall_unlink(path); }

int mkdir(const char *path, mode_t mode) {
  return syscall_mkdir(path, (uint32_t)mode);
}

int rmdir(const char *path) { return syscall_rmdir(path); }

// ============================================================================
// IPC
// ============================================================================

int pipe(int pipefd[2]) { return syscall_pipe(pipefd); }

int shmget(key_t key, size_t size, int shmflg) {
  return syscall_shmget((uint32_t)key, (uint32_t)size, shmflg);
}

void *shmat(int shmid, const void *shmaddr, int shmflg) {
  return syscall_shmat(shmid);
}

int shmdt(const void *shmaddr) { return syscall_shmdt((void *)shmaddr); }

// =
// ============================================================================
// Time
// ============================================================================

int nanosleep(const struct timespec *req, struct timespec *rem) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_NANOSLEEP), "b"(req), "c"(rem));
  return res;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_CLOCK_GETTIME), "b"(clk_id), "c"(tp));
  return res;
}
}
