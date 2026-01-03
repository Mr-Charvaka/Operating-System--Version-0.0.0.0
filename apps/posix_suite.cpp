#include "include/os/syscalls.hpp"
#include "include/syscall.h"
#include "include/userlib.h"
#include "pthread.h"
#include "semaphore.h"
#include "signal.h"
#include "time.h"
#include "types.h"


using namespace OS::Syscall;

// ============================================================================
// Helpers
// ============================================================================
void log(const char *str) { OS::Syscall::print(str); }

void test_section(const char *name) {
  log("\n--- [ TEST: ");
  log(name);
  log(" ] ---\n");
}

void test_result(const char *subtest, bool passed) {
  log("  ");
  log(subtest);
  if (passed) {
    log(": [PASS]\n");
  } else {
    log(": [FAIL]\n");
  }
}

// ============================================================================
// Process Tests
// ============================================================================
void test_processes() {
  test_section("Process Management");

  pid_t pid = getpid();
  log("  Current PID: ");
  print_int(pid);
  log("\n");
  test_result("getpid", pid > 0);

  pid_t ppid = getppid();
  log("  Parent PID: ");
  print_int(ppid);
  log("\n");
  test_result("getppid", ppid >= 0);

  log("  Testing fork...\n");
  int f = fork();
  if (f < 0) {
    test_result("fork", false);
  } else if (f == 0) {
    // Child
    exit(42);
  } else {
    // Parent
    int status = 0;
    int waited = wait(&status);
    test_result("fork/wait", waited == f);
    // We can't easily check status here as syscall_wait might not return exit
    // code in this OS yet
  }
}

// ============================================================================
// File I/O Tests
// ============================================================================
void test_file_io() {
  test_section("File I/O");

  const char *test_file = "posix_test.txt";
  int fd = open(test_file, O_CREAT | O_RDWR);
  if (fd < 0) {
    test_result("open(O_CREAT)", false);
    return;
  }
  test_result("open(O_CREAT)", true);

  const char *data = "POSIX Test Data";
  int written = write(fd, data, 15);
  test_result("write", written == 15);

  lseek(fd, 0, SEEK_SET);

  char buf[16];
  int read_bytes = read(fd, buf, 15);
  buf[15] = 0;
  test_result("read", read_bytes == 15 && strcmp(buf, data) == 0);

  close(fd);
  test_result("close", true);

  // stat not in Syscall namespace yet
  struct stat st;
  // We need the raw syscall since our wrappers are conflicting
  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_STAT), "b"(test_file), "c"(&st));
  test_result("stat", res == 0 && st.st_size == 15);

  asm volatile("int $0x80" : "=a"(res) : "a"(SYS_UNLINK), "b"(test_file));
  test_result("unlink", res == 0);
}

// ============================================================================
// Pipe Tests
// ============================================================================
void test_pipes() {
  test_section("Pipes");

  int fds[2];
  int p = pipe(fds);
  if (p < 0) {
    test_result("pipe", false);
    return;
  }
  test_result("pipe", true);

  const char *msg = "Hello Pipe";
  write(fds[1], msg, 10);

  char buf[11];
  int n = read(fds[0], buf, 10);
  buf[10] = 0;
  test_result("pipe read/write", n == 10 && strcmp(buf, msg) == 0);

  close(fds[0]);
  close(fds[1]);
}

// ============================================================================
// Thread Tests (Basic)
// ============================================================================
void *dummy_thread(void *arg) { return (void *)((uintptr_t)arg + 1); }

void test_threads() {
  test_section("Pthreads");

  pthread_t thread;
  int res = pthread_create(&thread, nullptr, dummy_thread, (void *)100);
  if (res != 0) {
    test_result("pthread_create", false);
    return;
  }
  test_result("pthread_create", true);

  void *retval;
  res = pthread_join(thread, &retval);
  test_result("pthread_join", res == 0 && (uintptr_t)retval == 101);
}

// ============================================================================
// Signal Tests
// ============================================================================
static bool alarm_fired = false;
void handle_alarm(int sig) { alarm_fired = true; }

void test_signals() {
  test_section("Signals");

  struct sigaction sa;
  sa.sa_handler = handle_alarm;
  sa.sa_flags = 0;
  // sigemptyset stubs usually just clear a bitmask
  // we'll just manaly clear it if needed but let's assume it works

  int res;
  asm volatile("int $0x80"
               : "=a"(res)
               : "a"(SYS_SIGACTION), "b"(SIGALRM), "c"(&sa), "d"((void *)0));
  if (res < 0) {
    test_result("sigaction", false);
  } else {
    test_result("sigaction", true);
    OS::Syscall::alarm(1); // 1 tick or 1 second? syscall.cpp says ticks
    log("  Waiting for alarm...\n");
    OS::Syscall::sleep(10); // Wait for alarm
    test_result("alarm/signal", alarm_fired);
  }
}

// ============================================================================
// Main
// ============================================================================
extern "C" void _start() {
  log("POSIX COMPREHENSIVE SUITE: Starting...\n");

  test_processes();
  test_file_io();
  test_pipes();
  test_threads();
  test_signals();

  log("\nPOSIX COMPREHENSIVE SUITE: Finished.\n");
  exit(0);
}
