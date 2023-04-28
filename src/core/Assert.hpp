#ifndef SNOWFLAKE_ASSERT_HPP
#define SNOWFLAKE_ASSERT_HPP

#include "pch.hpp"
#include "core/Logging.hpp"

// --- STATIC ASSERT --- //
//#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
//  #define STATIC_ASSERT _Static_assert
//#elif defined(COMPILER_MSC) || defined(COMPILER_GNU)
//  #define STATIC_ASSERT static_assert
//#else
//  #error Unknown Static Assert for current compiler!
//#endif

#define STATIC_ASSERT static_assert

// --- ASSERT --- //
#if defined(_DEBUG) || defined(_REL_DEBUG)
  #define ASSERT(expr)                                   \
    {                                                    \
      if (expr) {}                                       \
      else {                                             \
        LOG_IMMEDIATE(                                   \
          ::Logging::LogLevel::LOG_LEVEL_FATAL,          \
          "Assertion Failure: %s\n\tFile: %s, Line: %d", \
          #expr, __FILE__, __LINE__);                    \
        DEBUG_BREAK();                                   \
      }                                                  \
    }

  #define ASSERT_MSG(expr, msg)                                         \
    {                                                                   \
      if (expr) {}                                                      \
      else {                                                            \
        LOG_IMMEDIATE(                                                  \
          ::Logging::LogLevel::LOG_LEVEL_FATAL,                         \
          "Assertion Failure: %s\n\tMessage: %s\n\tFile: %s, Line: %d", \
          #expr, msg, __FILE__, __LINE__                                \
        );                                                              \
        DEBUG_BREAK();                                                  \
      }                                                                 \
    }

#else
  #define ASSERT(expr)
  #define ASSERT_MSG(expr, msg)
#endif

#endif //SNOWFLAKE_ASSERT_HPP
