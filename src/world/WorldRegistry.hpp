#ifndef SNOWFLAKE_WORLD_REGISTRY_HPP
#define SNOWFLAKE_WORLD_REGISTRY_HPP

#include "pch.hpp"
#include "containers/FArray.hpp"
#include "world/World.hpp"

// worlds
#include "world/worlds/DissWorld.hpp"

#define REGISTER_WORLD(init, shutdown, update) \
  World { .initFunc = (init), .shutdownFunc = (shutdown), .updateFunc = (update) }

constexpr const World WORLD_REGISTRY[]
  {
    REGISTER_WORLD(DissWorld::Init, DissWorld::Shutdown, DissWorld::Update),
  };

constexpr const u32 WORLD_COUNT = ARRAY_SIZE(WORLD_REGISTRY);

#endif //SNOWFLAKE_WORLD_REGISTRY_HPP
