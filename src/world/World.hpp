#ifndef SNOWFLAKE_WORLD_HPP
#define SNOWFLAKE_WORLD_HPP

#include "pch.hpp"

// Forward Declarations
namespace ECS { struct Manager; }

struct World
{
  typedef void (*WorldFuncPtr)(ECS::Manager& ecs);

  WorldFuncPtr initFunc;
  WorldFuncPtr shutdownFunc;
  WorldFuncPtr updateFunc;
};

#endif //SNOWFLAKE_WORLD_HPP
