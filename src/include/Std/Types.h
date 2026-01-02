#pragma once

#include <stddef.h>
#include <stdint.h>


namespace Std {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t size_t;
typedef uintptr_t uintptr_t;

} // namespace Std

using Std::i16;
using Std::i32;
using Std::i64;
using Std::i8;
using Std::size_t;
using Std::u16;
using Std::u32;
using Std::u64;
using Std::u8;
using Std::uintptr_t;

#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define NAKED __attribute__((naked))
