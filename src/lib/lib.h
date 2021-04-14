
#pragma once

#include <cfloat>
#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include <utility>
#include <atomic>
#include <limits>
#include <mutex>
#include <compare>
#include <new>
#include <thread>

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

typedef uintptr_t uptr;
typedef intptr_t iptr;

typedef float f32;
typedef double f64;

typedef size_t usize;

#define null nullptr

#include "math.h"

#include "alloc_before.h"

#include "string_before.h"

#include "log_before.h"

#include "types.h"

#include "printf_before.h"

#include "3d_math.h"

#include "pair.h"

#include "vec.h"

#include "array.h"

#include "stack.h"

#include "queue.h"

#include "heap.h"

#include "hash.h"

#include "map.h"

#include "log_after.h"

#include "alloc_after.h"

#include "string_after.h"

#include "printf_after.h"
