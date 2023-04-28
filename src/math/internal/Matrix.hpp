#ifndef SNOWFLAKE_MATRIX_HPP
#define SNOWFLAKE_MATRIX_HPP

#include "pch.hpp"

#include "math/internal/Quaternion.hpp"

namespace Math
{

  /** @brief Creates a TRS Matrix from a position, rotation (quaternion) and scale. */
  INLINE glm::mat4x4 CreateTRSMatrix(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
  {
    const glm::mat4x4 identity = glm::identity<glm::mat4x4>();

    const glm::mat4x4 translateMatrix = glm::translate(identity, position);
    const glm::mat4x4 rotationMatrix  = glm::mat4_cast(rotation);
    const glm::mat4x4 scaleMatrix     = glm::scale(identity, scale);

    return translateMatrix * rotationMatrix * scaleMatrix;
  }

  /** @brief Creates a TRS Matrix from a position, euler angle rotation and scale. */
  INLINE glm::mat4x4 CreateTRSMatrix(const glm::vec3& position, const glm::vec3& eulerAngles, const glm::vec3& scale)
  {
    const glm::quat rotation = Math::EulerAnglesToQuat(eulerAngles);
    return CreateTRSMatrix(position, rotation, scale);
  }

  /** @brief Calculate the trace of a 2x2 matrix */
  INLINE f32 MatrixTrace(const glm::mat2x2& mat)
  {
    return mat[0][0] + mat[1][1];
  }

  /** @brief Calculate the trace of a 3x3 matrix */
  INLINE f32 MatrixTrace(const glm::mat3x3& mat)
  {
    return mat[0][0] + mat[1][1] + mat[2][2];
  }

  /** @brief Calculate the trace of a 4x4 matrix */
  INLINE f32 MatrixTrace(const glm::mat4x4& mat)
  {
    return mat[0][0] + mat[1][1] + mat[2][2] + mat[3][3];
  }

  /** @brief Calculate the determinant of a 3x3 matrix */
  INLINE f32 MatrixDeterminant(const glm::mat3x3& mat)
  {
    return (mat[0][0] * mat[1][1] * mat[2][2]) +
           (mat[0][1] * mat[1][2] * mat[2][0]) +
           (mat[0][2] * mat[1][0] * mat[2][1]) -
           (mat[0][2] * mat[1][1] * mat[2][0]) -
           (mat[0][0] * mat[1][2] * mat[2][1]) -
           (mat[0][1] * mat[1][0] * mat[2][2]);
  }

} // namespace Math

#endif //SNOWFLAKE_MATRIX_HPP
