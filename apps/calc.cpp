#include "include/os/ipc.hpp"
#include "include/os/syscalls.hpp"

// Minimal calculator logic
int eval_expression(const char *expr) {
  // Parser placeholder - for now just returns 42
  return 42;
}

extern "C" void _start() {
  OS::Syscall::print("Calc App: Starting...\n");

  OS::IPCClient app;
  if (!app.connect()) {
    OS::Syscall::exit(1);
  }

  if (!app.create_window("Calculator", 250, 350)) {
    OS::Syscall::exit(1);
  }

  app.fill_rect(0, 0, 250, 350, 0xFF202020);

  // Draw display area
  app.fill_rect(10, 10, 230, 50, 0xFFFFFFFF);

  // Draw buttons (visual only for now)
  int btn_w = 50;
  int btn_h = 50;
  int gap = 10;
  int start_y = 70;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      app.fill_rect(10 + col * (btn_w + gap), start_y + row * (btn_h + gap),
                    btn_w, btn_h, 0xFF505050);
    }
  }

  app.flush();

  while (true) {
    OS::Syscall::sleep(1000);
  }
}
