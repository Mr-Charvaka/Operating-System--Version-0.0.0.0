#pragma once

namespace Std {

template <typename T> struct RemoveReference {
  using Type = T;
};

template <typename T> struct RemoveReference<T &> {
  using Type = T;
};

template <typename T> struct RemoveReference<T &&> {
  using Type = T;
};

template <typename T>
constexpr typename RemoveReference<T>::Type &&move(T &&arg) {
  return static_cast<typename RemoveReference<T>::Type &&>(arg);
}

// Forward Decls
class String;

template <typename T> class Vector;

template <typename T> class RefPtr;

template <typename T> class OwnPtr;

} // namespace Std

using Std::move;
