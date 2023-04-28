/**
 * @file types.h 
 * 
 * @brief Common types used throughout the application.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <float.h>
#include "common/detection/compiler_detection.h"
#include "common/detection/language_detection.h"
#include "common/detection/platform_detection.h"

// --- TYPEDEF --- //
// Signed int
typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Unsigned int
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Floating point
typedef float  f32;
typedef double f64;

// Boolean
#if defined(LANGUAGE_C) 
  #include <stdbool.h>
  #define bool _Bool
  #define true  1
  #define false 0
#endif

typedef i32  b32;
typedef bool b8;

// Char
typedef unsigned char uchar;
typedef signed char   schar;

// Byte
typedef uchar byte;
typedef schar sbyte;

// --- TYPEOF --- //
#if defined(LANGUAGE_C) 
  #if defined(COMPILER_CLANG)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wkeyword-macro"
    #define typeof(var) __typeof__(var)
    #pragma clang diagnostic pop
  #elif defined(COMPILER_GCC)
    #define typeof(var) __typeof__(var)
  #else // MSVC
    #define typeof(var) decltype(var)
  #endif
#elif defined(LANGUAGE_CPP)
  #define typeof(var) decltype(var)
#endif

// --- MEMORY DEFINES --- //
#define  B(val) ((val))
#define KB(val) ((val)      << 10)
#define MB(val) ((val)      << 20)
#define GB(val) (((u64)val) << 30)
#define TB(val) (((u64)val) << 40)

#if defined(COMPILER_MSC)
#define KiB(val) (val * 1024i64)
#define MiB(val) (val * 1048576i64)
#define GiB(val) (val * 1073741824i64)
#else
#define KiB(val) (val * 1024ull)
#define MiB(val) (val * 1048576ull)
#define GiB(val) (val * 1073741824ull)
#endif

// --- TYPES MIN/MAX --- //
// Signed int
#define I8_MAX  INT8_MAX
#define I8_MIN  INT8_MIN

#define I16_MAX INT16_MAX
#define I16_MIN INT16_MIN

#define I32_MAX INT32_MAX
#define I32_MIN INT32_MIN

#define I64_MAX INT64_MAX
#define I64_MIN INT64_MIN

// Unsigned int
#define U8_MAX  UINT8_MAX
#define U8_MIN  0

#define U16_MAX UINT16_MAX
#define U16_MIN 0

#define U32_MAX UINT32_MAX
#define U32_MIN 0

#define U64_MAX UINT64_MAX
#define U64_MIN 0

// Floating point
#define F32_MAX FLT_MAX
#define F32_MIN FLT_MIN
#define F32_EPSILON FLT_EPSILON

#define F64_MAX DBL_MAX
#define F64_MIN DBL_MIN
#define F64_EPSILON DBL_EPSILON

#endif // TYPES_H
