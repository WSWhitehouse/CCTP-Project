#ifndef SNOWFLAKE_COMMON_HPP
#define SNOWFLAKE_COMMON_HPP

#include "pch.hpp"

namespace Math
{

  [[nodiscard]] INLINE f32 Lerp(f32 a, f32 b, f32 t)
  {
    return ((1.0f - t) * a) + (b * t);
  }

  [[nodiscard]] INLINE glm::vec2 Lerp(const glm::vec2& a, const glm::vec2& b, f32 t)
  {
    return glm::vec2(Lerp(a.x, b.x, t),
                     Lerp(a.y, b.y, t));
  }

  [[nodiscard]] INLINE glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, f32 t)
  {
    return glm::vec3(Lerp(a.x, b.x, t),
                     Lerp(a.y, b.y, t),
                     Lerp(a.z, b.z, t));
  }

} // namespace Math

#endif //SNOWFLAKE_COMMON_HPP
