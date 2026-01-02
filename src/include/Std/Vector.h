#pragma once

#include <Std/Assertions.h>
#include <Std/Forward.h>
#include <Std/Types.h>


extern "C" {
void *kmalloc(size_t);
}

namespace Std {

template <typename T> class Vector {
public:
  Vector() : m_data(nullptr), m_size(0), m_capacity(0) {}

  ~Vector() {
    // if (m_data) kfree(m_data);
  }

  void push_back(const T &value) {
    ensure_capacity(m_size + 1);
    new (&m_data[m_size]) T(value);
    m_size++;
  }

  void push_back(T &&value) {
    ensure_capacity(m_size + 1);
    new (&m_data[m_size]) T(move(value));
    m_size++;
  }

  T &operator[](size_t index) {
    ASSERT(index < m_size);
    return m_data[index];
  }

  const T &operator[](size_t index) const {
    ASSERT(index < m_size);
    return m_data[index];
  }

  size_t size() const { return m_size; }

private:
  void ensure_capacity(size_t new_capacity) {
    if (new_capacity <= m_capacity)
      return;

    size_t next_capacity = m_capacity == 0 ? 4 : m_capacity * 2;
    if (next_capacity < new_capacity)
      next_capacity = new_capacity;

    T *new_data = (T *)kmalloc(next_capacity * sizeof(T));

    for (size_t i = 0; i < m_size; i++) {
      new (&new_data[i]) T(move(m_data[i]));
      m_data[i].~T();
    }

    // if (m_data) kfree(m_data);
    m_data = new_data;
    m_capacity = next_capacity;
  }

  T *m_data;
  size_t m_size;
  size_t m_capacity;
};

} // namespace Std

using Std::Vector;
