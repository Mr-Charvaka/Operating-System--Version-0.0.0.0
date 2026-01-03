#include "include/os/ipc.hpp"
#include "include/os/syscalls.hpp"

extern "C" void _start() {
  OS::Syscall::print("Hello App: Starting...\n");

  OS::IPCClient app;
  if (!app.connect()) {
    OS::Syscall::print("Hello App: Failed to connect to WindowServer.\n");
    OS::Syscall::exit(1);
  }

  if (!app.create_window("Hello C++", 300, 200)) {
    OS::Syscall::print("Hello App: Failed to create window.\n");
    OS::Syscall::exit(1);
  }

  // specific drawing
  auto *fb = app.get_buffer();
  int w = app.get_width();
  int h = app.get_height();

  // Simple background
  app.fill_rect(0, 0, w, h, 0xFF333333); // Dark Grey

  // Draw a square
  app.fill_rect(50, 50, 100, 100, 0xFFFF0000); // Red

  app.flush();

  while (1) {
    OS::Syscall::sleep(100);
    // Event loop placeholder
  }
}
