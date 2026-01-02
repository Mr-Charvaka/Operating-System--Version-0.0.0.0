#pragma once

#include <Std/Types.h>

extern "C" void serial_log(const char *msg);

namespace Std {

void crash(const char *msg, const char *file, int line, const char *function);

}

#ifdef DEBUG
#define ASSERT(x)                                                              \
  if (!(x)) {                                                                  \
    Std::crash("ASSERTION FAILED: " #x, __FILE__, __LINE__,                    \
               __PRETTY_FUNCTION__);                                           \
  }
#define ASSERT_NOT_REACHED()                                                   \
  Std::crash("ASSERT_NOT_REACHED", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
#define ASSERT(x)
#define ASSERT_NOT_REACHED()                                                   \
  while (1)                                                                    \
  __builtin_unreachable()
#endif

// Implementation usually goes in a .cpp file, but for now we put it inline or
// assume linked later. We'll add Assertions.cpp to the build.
