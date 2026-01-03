#include "include/syscall.h"
#include "pthread.h"
#include "semaphore.h"
#include "signal.h"
#include "time.h"

// ============================================================================
// Simple printing wrapper
// ============================================================================
void print_str(const char *str) {
  asm volatile("int $0x80" ::"a"(SYS_PRINT), "b"(str));
}

// ============================================================================
// Pthread Test
// ============================================================================
void *thread_func(void *arg) {
  print_str("PTHREAD: Thread running!\n");
  return (void *)42;
}

void test_pthreads() {
  print_str("PTHREAD: Starting test...\n");
  pthread_t thread;
  if (pthread_create(&thread, 0, thread_func, 0) == 0) {
    print_str("PTHREAD: Thread created successfully.\n");
    void *retval;
    pthread_join(thread, &retval);
    print_str("PTHREAD: Thread joined.\n");
  } else {
    print_str("PTHREAD: Failed to create thread.\n");
  }
}

// ============================================================================
// Semaphore Test
// ============================================================================
void test_semaphores() {
  print_str("SEM: Starting test...\n");
  sem_t sem;
  if (sem_init(&sem, 0, 1) == 0) {
    print_str("SEM: Initialized.\n");
    sem_wait(&sem);
    print_str("SEM: Wait (lock) successful.\n");
    sem_post(&sem);
    print_str("SEM: Post (unlock) successful.\n");
    sem_destroy(&sem);
    print_str("SEM: Destroyed.\n");
  } else {
    print_str("SEM: Failed to initialize.\n");
  }
}

// ============================================================================
// Signal Test
// ============================================================================
void signal_handler(int sig) { print_str("SIGNAL: Received alarm signal!\n"); }

void test_signals() {
  print_str("SIGNAL: Starting test...\n");
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGALRM, &sa, 0) == 0) {
    print_str("SIGNAL: Handler registered.\n");
    alarm(1);
    print_str("SIGNAL: Alarm set for 1 second.\n");
    // We'll wait for it via nanosleep or just busy wait
  } else {
    print_str("SIGNAL: Failed to register handler.\n");
  }
}

// ============================================================================
// Main Entry
// ============================================================================
extern "C" void _start() {
  print_str("POSIX TEST APP: Starting...\n");

  test_pthreads();
  test_semaphores();
  test_signals();

  print_str("POSIX TEST APP: Tests completed.\n");

  // Exit
  asm volatile("int $0x80" ::"a"(SYS_EXIT), "b"(0));
  while (1)
    ;
}
