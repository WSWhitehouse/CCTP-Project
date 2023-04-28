#ifndef SNOWFLAKE_PLANE_HPP
#define SNOWFLAKE_PLANE_HPP

#include "pch.hpp"
#include "math/Math.hpp"

/**
* @brief A simple 3D implicitly defined plane.
*/
struct Plane
{
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 normal;

  /**
  * @brief Calculate the signed distance from the point to the plane.
  * @param point Point to calculate distance from.
  * @return Signed distance from point.
  */
  [[nodiscard]] INLINE f32 SignedDistanceFromPoint(glm::vec3 point) const
  {
    // https://iquilezles.org/articles/distfunctions/
    const glm::vec3 diff = point - position;
    return glm::dot(normal, diff);
  }

  /**
  * @brief Checks if a line has intersected with the plane. Finds the point
  * along the line (point1 and point2) that intersects with the plane.
  * @param point1 Start of line
  * @param point2 End of line
  * @param out_intersectionPos Intersection point along line.
  * @param out_intersectionT Interpolation value across line.
  * @return True when there is an intersection; false otherwise.
  */
  [[nodiscard]] INLINE b8 LineIntersection(const glm::vec3& point1, const glm::vec3& point2,
                                             glm::vec3& out_intersectionPos,
                                             f32& out_intersectionT) const
  {
    const f32 t = glm::dot(normal, position - point1) /
                  glm::dot(normal, point2   - point1);

    out_intersectionT = t;

    if (t < 0.0f || t > 1.0f) return false;

    out_intersectionPos = Math::Lerp(point1, point2, t);
    return true;
  }
};

#endif //SNOWFLAKE_PLANE_HPP
