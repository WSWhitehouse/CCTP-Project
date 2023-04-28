#ifndef SNOWFLAKE_SPRITE_SYSTEM_HPP
#define SNOWFLAKE_SPRITE_SYSTEM_HPP

#include "pch.hpp"
#include "ecs/ECS.hpp"

#include "ecs/components/Sprite.hpp"
#include "ecs/components/Transform.hpp"

namespace SpriteSystem
{

  void INLINE Update(ECS::Manager& ecs)
  {
    using namespace ECS;

    ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<Sprite>();
    for (u32 i = 0; i < sparseSet->componentCount; ++i)
    {
      ComponentData<Sprite>& componentData = ((ComponentData<Sprite>*)sparseSet->componentArray)[i];
      Sprite& sprite = componentData.component;

      if (!ecs.HasComponent<Transform>(componentData.entity)) return;
      Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

      if (sprite.billboard != Sprite::BillboardType::DISABLED)
      {
        // NOTE(WSWhitehouse): By default we do Cylindrical billboarding as
        // its also required by the spherical method...
        transform->matrix[0][0] = 1.0f;
        transform->matrix[0][1] = 0.0f;
        transform->matrix[0][2] = 0.0f;

        transform->matrix[2][0] = 0.0f;
        transform->matrix[2][1] = 0.0f;
        transform->matrix[2][2] = 1.0f;

        if (sprite.billboard == Sprite::BillboardType::SPHERICAL)
        {
          transform->matrix[1][0] = 0.0f;
          transform->matrix[1][1] = 1.0f;
          transform->matrix[1][2] = 0.0f;
        }
      }
    }
  }

} // namespace SpriteSystem

#endif //SNOWFLAKE_SPRITE_SYSTEM_HPP
