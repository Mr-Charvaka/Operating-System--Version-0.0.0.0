#pragma once

#include <Std/Assertions.h>
#include <Std/Forward.h>
#include <Std/Types.h>


extern "C" {
size_t strlen(const char *);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
int strcmp(const char *, const char *);
void *kmalloc(size_t);
void kfree(void *);
}

namespace Std {

class String {
public:
  String() : m_data(nullptr), m_length(0) {}

  String(const char *cstr) {
    if (!cstr) {
      m_data = nullptr;
      m_length = 0;
      return;
    }
    m_length = strlen(cstr);
    m_data = (char *)kmalloc(m_length + 1);
    strcpy(m_data, cstr);
  }

  String(const String &other) {
    if (other.m_data) {
      m_length = other.m_length;
      m_data = (char *)kmalloc(m_length + 1);
      strcpy(m_data, other.m_data);
    } else {
      m_data = nullptr;
      m_length = 0;
    }
  }

  String(String &&other) {
    m_data = other.m_data;
    m_length = other.m_length;
    other.m_data = nullptr;
    other.m_length = 0;
  }

  ~String() {
    if (m_data) {
      // kfree(m_data); // TODO: Enable when kfree is ready
    }
  }

  String &operator=(const String &other) {
    if (this == &other)
      return *this;
    // if (m_data) kfree(m_data);
    if (other.m_data) {
      m_length = other.m_length;
      m_data = (char *)kmalloc(m_length + 1);
      strcpy(m_data, other.m_data);
    } else {
      m_data = nullptr;
      m_length = 0;
    }
    return *this;
  }

  const char *c_str() const { return m_data ? m_data : ""; }
  size_t length() const { return m_length; }

  static String format(const char *fmt, ...); // To verify later

private:
  char *m_data;
  size_t m_length;
};

} // namespace Std

using Std::String;
