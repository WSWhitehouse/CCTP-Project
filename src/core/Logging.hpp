#ifndef SNOWFLAKE_LOGGING_HPP
#define SNOWFLAKE_LOGGING_HPP

#include "pch.hpp"

#if defined(COMPILER_CLANG)
  #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

namespace Logging
{
  enum LogLevel
  {
    LOG_LEVEL_FATAL   = 0,
    LOG_LEVEL_ERROR   = 1,
    LOG_LEVEL_WARN    = 2,
    LOG_LEVEL_INFO    = 3,
    LOG_LEVEL_DEBUG   = 4,
    LOG_LEVEL_PROFILE = 5
  };

  /**
  * @brief Initialises the multi-threaded Logging system. This
  * function must be called before logs will be printed to the
  * console.
  * @return True on success; false otherwise.
  */
  b8 Init();

  /**
  * @brief Safely shuts down the multi-threaded logging system,
  * ensures any logs in the queue are handled before shut down.
  */
  void Shutdown();

  /**
  * @brief Adds a message to the queue ready to be handled by the
  * logging system. Handles variadic arguments, and gets the
  * current time stamp. Prefer to use the Logging Macros over this
  * function.
  * @param level Logging level severity.
  * @param msg Message to output.
  * @param ... VA Args to be processed.
  */
  void LogMessage(const Logging::LogLevel level, const char* msg, ...);

  /**
  * @brief Logs a message immediately, this is not recommended for
  * logging as it can break the log queue and cause out of order
  * flushing from other threads.
  * @param level Logging level severity.
  * @param msg Message to output.
  * @param ... VA Args to be processed.
  */
  void LogMessageImmediate(const Logging::LogLevel level, const char* msg, ...);

} // namespace Logging

#define LOGGING_ENABLE
#if defined(LOGGING_ENABLE)
  #define LOG_FATAL(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_FATAL, "%s:%i : %s", __FILE__, __LINE__, msg, ##__VA_ARGS__)
  #define LOG_ERROR(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_ERROR, msg, ##__VA_ARGS__)
  #define LOG_WARN(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_WARN, msg, ##__VA_ARGS__)
  #define LOG_INFO(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_INFO, msg, ##__VA_ARGS__)
  #define LOG_DEBUG(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_DEBUG, msg, ##__VA_ARGS__)
  #define LOG_PROFILE(msg, ...) ::Logging::LogMessage(::Logging::LogLevel::LOG_LEVEL_PROFILE, msg, ##__VA_ARGS__)
  #define LOG_IMMEDIATE(level, msg, ...) ::Logging::LogMessageImmediate(level, msg, ##__VA_ARGS__)
#else
  #define LOG_FATAL(msg, ...)
  #define LOG_ERROR(msg, ...)
  #define LOG_WARN(msg, ...)
  #define LOG_INFO(msg, ...)
  #define LOG_DEBUG(msg, ...)
  #define LOG_PROFILE(msg, ...)
  #define LOG_IMMEDIATE(level, msg, ...)
#endif
#endif //SNOWFLAKE_LOGGING_HPP
