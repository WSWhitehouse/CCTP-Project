#ifndef SNOWFLAKE_PROFILER_HPP
#define SNOWFLAKE_PROFILER_HPP

#include "core/Logging.hpp"
#include "Platform.hpp"

#define PROFILE_FUNC ProfileScope profile_func{__FUNCTION__};

struct ProfileScope
{
  INLINE explicit ProfileScope(const char* scopeName) noexcept
  {
    _scopeName = scopeName;
    _startTime = Platform::GetTime();
  }

  INLINE ~ProfileScope() noexcept
  {
    const f64 endTime     = Platform::GetTime();
    const f64 timeElapsed = endTime - _startTime;

    LOG_PROFILE("ProfileScope::%s elapsed time: %f", _scopeName, timeElapsed);
  }

private:
  const char* _scopeName;
  f64 _startTime;
};

#endif //SNOWFLAKE_PROFILER_HPP
