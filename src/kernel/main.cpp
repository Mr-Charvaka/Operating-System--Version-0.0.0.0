#include "../drivers/serial.h"

class TestClass {
public:
  TestClass() { serial_log("[C++] TestClass Constructor called!"); }
  ~TestClass() { serial_log("[C++] TestClass Destructor called!"); }
  void hello() { serial_log("[C++] TestClass::hello() called!"); }
};

extern "C" void cpp_kernel_entry() {
  serial_log("[C++] Entered cpp_kernel_entry");

  // Convert a simple object on heap
  TestClass *t = new TestClass();
  t->hello();

  // We can't delete yet because kfree isn't implemented!
  // delete t;

  serial_log("[C++] Exiting cpp_kernel_entry");
}
