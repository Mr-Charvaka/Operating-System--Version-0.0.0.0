#include "include/os/ipc.hpp"
#include "include/os/syscalls.hpp"

extern "C" void _start() {
  OS::IPCClient app;

  if (!app.connect()) {
    OS::Syscall::print("Demo IPC: Failed to connect to WindowServer.\n");
    OS::Syscall::exit(1);
  }

  if (!app.create_window("IPC C++ Demo", 400, 300)) {
    OS::Syscall::print("Demo IPC: Failed to create window.\n");
    OS::Syscall::exit(1);
  }

  int width = app.get_width();
  int height = app.get_height();

  int x = 50;
  int y = 50;
  int dx = 2;
  int dy = 2;
  int size = 40;

  while (true) {
    // Clear background
    app.fill_rect(0, 0, width, height, 0xFFF0F0F0);

    // Draw animated square
    app.fill_rect(x, y, size, size, 0xFFFF0000);

    // Update position
    x += dx;
    y += dy;

    if (x <= 0 || x + size >= width)
      dx = -dx;
    if (y <= 0 || y + size >= height)
      dy = -dy;

    // Flush to screen
    app.flush();

    // Small delay
    for (volatile int i = 0; i < 500000; i++)
      ;
  }
}
