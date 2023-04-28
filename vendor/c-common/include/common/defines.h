/**
 * @file defines.h
 * 
 * @brief Provides common definitions for the application.
 */

#ifndef DEFINES_H
#define DEFINES_H

#include "common/detection/compiler_detection.h"

// --- INLINING --- //
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC) || defined(COMPILER_GNU)
  /** @brief Force inline function */
  #define INLINE __attribute__((always_inline)) inline

  /** @brief Force function to be no-inline */
  #define NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSC)
  /** @brief Force inline function */
  #define INLINE __forceinline

  /** @brief Force function to be no-inline */
  #define NOINLINE __declspec(noinline)
#else 
  /** @brief Force inline function */
  #define INLINE inline

  /** @brief Force function to be no-inline */
  #define NOINLINE
#endif

// --- DEBUG BREAK --- //
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC) || defined(COMPILER_GNU)
  /** @brief Stops execution of the program when this macro is hit (equivalent to a breakpoint) */
  #define DEBUG_BREAK() __builtin_trap()
#elif defined(COMPILER_MSC)
  #include <intrin.h>
  /** @brief Stops execution of the program when this macro is hit (equivalent to a breakpoint) */
  #define DEBUG_BREAK() __debugbreak()
#else 
  /** @brief Stops execution of the program when this macro is hit (equivalent to a breakpoint) */
  #define DEBUG_BREAK()
#endif

// --- UNUSED VAR --- //
/** @brief Stop compiler warnings/errors on variables that aren't being used */
#define UNUSED(val) ((void)(val))

#endif // DEFINES_H
