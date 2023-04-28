#ifndef SNOWFLAKE_WORLD_MANAGER_HPP
#define SNOWFLAKE_WORLD_MANAGER_HPP

#include "pch.hpp"
#include "world/World.hpp"

typedef u32 WorldID;

namespace WorldManager
{
  void Init();
  void Shutdown();

  b8 LoadWorld(WorldID worldId);

  void BeginFrame();
  void UpdateWorld();
  void EndFrame();

} // namespace WorldManager


#endif //SNOWFLAKE_WORLD_MANAGER_HPP
