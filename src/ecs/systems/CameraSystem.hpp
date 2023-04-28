#ifndef SNOWFLAKE_CAMERA_SYSTEM_HPP
#define SNOWFLAKE_CAMERA_SYSTEM_HPP

#include "pch.hpp"

// Forward Declarations
namespace ECS { struct Manager; }

namespace CameraSystem
{

  void Update(ECS::Manager& ecs);

} // namespace CameraSystem

#endif //SNOWFLAKE_CAMERA_SYSTEM_HPP
