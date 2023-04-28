#ifndef SNOWFLAKE_FLY_CAM_SYSTEM_HPP
#define SNOWFLAKE_FLY_CAM_SYSTEM_HPP

#include "pch.hpp"

// Forward Declarations
namespace ECS { struct Manager; }

namespace FlyCamSystem
{

  void Update(ECS::Manager& ecs);

} // namespace FlyCamSystem

#endif //SNOWFLAKE_FLY_CAM_SYSTEM_HPP
