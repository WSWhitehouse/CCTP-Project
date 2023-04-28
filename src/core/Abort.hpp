#ifndef SNOWFLAKE_ABORT_HPP
#define SNOWFLAKE_ABORT_HPP

#include "pch.hpp"
#include "core/Logging.hpp"

// std includes
#include <cstdlib>

/**
* @brief Abort code used when aborting the
* program using the abort macro.
*/
enum AbortCode
{
  ABORT_CODE_FAILURE = EXIT_FAILURE,

  ABORT_CODE_MEMORY_ALLOC_FAILURE,
  ABORT_CODE_MEMORY_FREE_FAILURE,

  ABORT_CODE_ASSET_FAILURE,

  ABORT_CODE_VK_FAILURE,

  ABORT_CODE_ECS_FAILURE,
};

/**
* @brief Abort the application. Logs the error code and its
* string representation to the console before exiting. Only
* abort as a last resort, and not during gameplay/user code.
* @param exitcode Abort code. Must be of enum type "AbortCode"
*/
#define ABORT(exitcode)                                           \
do                                                                \
{                                                                 \
  LOG_IMMEDIATE(::Logging::LogLevel::LOG_LEVEL_FATAL,             \
                "App aborting with code %i (%s) (%s:%i)",         \
                (u16)(exitcode), #exitcode, __FILE__, __LINE__);  \
  ::std::exit((u16)(exitcode));                                   \
}                                                                 \
while(false)

#endif //SNOWFLAKE_ABORT_HPP
