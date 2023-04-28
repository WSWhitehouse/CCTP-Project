#ifndef SNOWFLAKE_TRANSFORM_SYSTEM_HPP
#define SNOWFLAKE_TRANSFORM_SYSTEM_HPP

#include "pch.hpp"

#include "ecs/ECS.hpp"
#include "ecs/components/Transform.hpp"

#include "math/Math.hpp"

namespace TransformSystem
{

  INLINE void Update(ECS::Manager& ecs)
  {
    using namespace ECS;

    ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<Transform>();
    for (u32 i = 0; i < sparseSet->componentCount; ++i)
    {
      ComponentData<Transform>& componentData = ((ComponentData<Transform>*)sparseSet->componentArray)[i];
      Transform& transform = componentData.component;

      transform.matrix = Math::CreateTRSMatrix(transform.position, transform.rotation, transform.scale);
    }
  }

} // namespace TransformSystem

#endif //SNOWFLAKE_TRANSFORM_SYSTEM_HPP
