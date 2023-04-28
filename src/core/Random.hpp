#ifndef SNOWFLAKE_RANDOM_HPP
#define SNOWFLAKE_RANDOM_HPP

#include "pch.hpp"

namespace Random
{

  f32 Range(f32 min, f32 max);
  i32 Range(i32 min, i32 max);
  u32 Range(u32 min, u32 max);

  b8 Bool(f32 truePercent = 0.5f);

} // namespace Random

#endif //SNOWFLAKE_RANDOM_HPP
