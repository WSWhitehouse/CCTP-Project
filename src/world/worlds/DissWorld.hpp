#ifndef SNOWFLAKE_DISS_WORLD_HPP
#define SNOWFLAKE_DISS_WORLD_HPP

#include "pch.hpp"

// Forward Declarations
namespace ECS { struct Manager; }

namespace DissWorld
{

  void Init(ECS::Manager& ecs);
  void Shutdown(ECS::Manager& ecs);
  void Update(ECS::Manager& ecs);

} // namespace DissWorld


#endif //SNOWFLAKE_DISS_WORLD_HPP
