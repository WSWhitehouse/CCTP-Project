#ifndef SNOWFLAKE_GIZMOS_HPP
#define SNOWFLAKE_GIZMOS_HPP

#include "pch.hpp"
#include "math/Math.hpp"

namespace Gizmos
{
  static constexpr const glm::vec3 DEFAULT_COLOUR = glm::vec3(0.0f, 1.0f, 0.0f);

  void Init();
  void Destroy();

  void EnableGizmos();
  void DisableGizmos();
  void ToggleGizmos();

  void DrawWireCube(glm::vec3 position, glm::vec3 extents, glm::quat rotation, glm::vec3 colour = DEFAULT_COLOUR);

  INLINE void DrawWireCube(glm::vec3 position, glm::vec3 extents, glm::vec3 eulerAngles = {0.0f, 0.0f, 0.0f}, glm::vec3 colour = DEFAULT_COLOUR)
  {
    const glm::quat rotation = Math::EulerAnglesToQuat(eulerAngles);
    DrawWireCube(position, extents, rotation, colour);
  }

} // namespace Gizmos


#endif //SNOWFLAKE_GIZMOS_HPP
