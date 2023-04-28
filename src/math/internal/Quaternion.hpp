#ifndef SNOWFLAKE_QUATERNION_HPP
#define SNOWFLAKE_QUATERNION_HPP

#include "pch.hpp"

namespace Math
{

  /**
  * @brief Returns a quaternion rotation that looks in the direction supplied.
  * Safely chooses the correct "up" vector to ensure NaNs aren't produced
  * in the rotation.
  * @param direction Direction to look.
  * @param up Default up direction. Default = {0.0f, 1.0f, 0.0f}.
  * @param altUp Alternate up direction. Default = {1.0f, 0.0f, 0.0f}.
  * @return Quaternion rotation looking towards the direction vector.
  */
  INLINE glm::quat QuatLookInDirection(glm::vec3 direction,
                                       const glm::vec3& up    = {0.0f, 1.0f, 0.0f},
                                       const glm::vec3& altUp = {1.0f, 0.0f, 0.0f})
  {
    const f32 directionLength = glm::length(direction);

    // Check if the direction is valid, also deals with NaN
    if(directionLength <= 0.0001f) return glm::identity<glm::quat>();

    // Normalize direction
    direction /= directionLength;

    // Is the up vector (nearly) parallel to direction?
    if(glm::abs(glm::dot(direction, up)) >= 0.9999f)
    {
      return glm::quatLookAt(direction, altUp);
    }

    return glm::quatLookAt(direction, up);
  }

  INLINE glm::quat EulerAnglesToQuat(glm::vec3 eulerAngles)
  {
    return glm::qua(glm::radians(eulerAngles));
  }

  INLINE glm::vec3 QuatToEulerAngles(glm::quat quaternion)
  {
    return glm::degrees(glm::eulerAngles(quaternion));
  }

  INLINE glm::mat3x3 QuatToMat3x3(glm::quat quaternion)
  {
    return glm::mat3_cast(quaternion);
  }

  INLINE glm::mat3x3 EulerAnglesToMat3x3(glm::vec3 eulerAngles)
  {
    return QuatToMat3x3(EulerAnglesToQuat(eulerAngles));
  }

} // namespace Math

#endif //SNOWFLAKE_QUATERNION_HPP
