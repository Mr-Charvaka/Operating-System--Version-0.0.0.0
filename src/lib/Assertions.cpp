#include <Std/Assertions.h>

namespace Std {

void crash(const char *msg, const char *file, int line, const char *function) {
  // We need a way to format strings. For now, just log raw parts.
  serial_log("\n\n*** KERNEL CRASH ***\n");
  serial_log(msg);
  serial_log("\nFile: ");
  serial_log(file);
  serial_log("\nFunction: ");
  serial_log(function);

  // Halt
  while (1) {
    asm volatile("cli; hlt");
  }
}

} // namespace Std
