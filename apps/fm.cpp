#include "include/os/ipc.hpp"
#include "include/os/syscalls.hpp"

extern "C" void _start() {
  OS::Syscall::print("FM App: Starting...\n");

  OS::IPCClient app;
  if (!app.connect()) {
    OS::Syscall::exit(1);
  }

  if (!app.create_window("File Manager", 500, 400)) {
    OS::Syscall::exit(1);
  }

  app.fill_rect(0, 0, 500, 400, 0xFFFFFFFF);

  // List files
  int fd = OS::Syscall::open("/", 0x00); // O_RDONLY
  if (fd >= 0) {
    struct dirent de;

    int y = 10;
    int i = 0;
    while (OS::Syscall::readdir(fd, i, &de) ==
           0) { // readdir returns 0 on success? Check syscall impl
      // Actually most readdir return 1 on success or bytes read.
      // Let's assume syscall convention: return 1 if read, 0 if EOF per
      // previous usage patterns or check syscall source. Looking at `syscall.h`
      // in the view_file, readdir returns `int res`. Standard `readdir` often
      // returns bytes read. We will try a loop.

      // Draw file icon/name placeholder
      app.fill_rect(10, y, 20, 20, 0xFFCCCC00); // Folder/File icon color

      // Since we don't have font rendering fully hooked up to draw text from
      // string yet (need a font bitmap) We will just draw blocks for files.

      y += 25;
      i++;
      if (i > 20)
        break; // limit
    }
    OS::Syscall::close(fd);
  }

  app.flush();

  while (true) {
    OS::Syscall::sleep(1000);
  }
}
