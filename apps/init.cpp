#include "include/os/syscalls.hpp"

extern "C" void _start() {
  OS::Syscall::print("INIT (C++): Starting All Applications...\n");

  // Give WindowServer time to bind /tmp/ws.sock
  OS::Syscall::sleep(20);

  // Spawn Hello
  if (OS::Syscall::fork() == 0) {
    const char *args[] = {"HELLO.ELF", nullptr};
    OS::Syscall::execve("HELLO.ELF", (char **)args, nullptr);
    OS::Syscall::exit(0);
  }

  // Spawn Calc
  if (OS::Syscall::fork() == 0) {
    const char *args[] = {"CALC.ELF", nullptr};
    OS::Syscall::execve("CALC.ELF", (char **)args, nullptr);
    OS::Syscall::exit(0);
  }

  // Spawn Demo IPC
  if (OS::Syscall::fork() == 0) {
    const char *args[] = {"DEMO_IPC.ELF", nullptr};
    OS::Syscall::execve("DEMO_IPC.ELF", (char **)args, nullptr);
    OS::Syscall::exit(0);
  }

  // Spawn POSIX Suite
  if (OS::Syscall::fork() == 0) {
    const char *args[] = {"POSIX_SU.ELF", nullptr};
    OS::Syscall::execve("POSIX_SU.ELF", (char **)args, nullptr);
    OS::Syscall::exit(0);
  }

  while (true) {
    OS::Syscall::wait(nullptr); // Reap zombies
  }
}
