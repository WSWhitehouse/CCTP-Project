#ifndef SNOWFLAKE_SPRITE_HPP
#define SNOWFLAKE_SPRITE_HPP

#include "pch.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "renderer/Texture2D.hpp"

struct Sprite
{
  enum class BillboardType
  {
    DISABLED = 0,
    SPHERICAL,
    CYLINDRICAL,
  };

  b8 render = true;

  Texture2D texture;
  u32 textureCount      = 0;
  u32 currentTexIndex   = 0;
  glm::vec2 textureSize = {0.0f, 0.0f};

  glm::vec3 colour        = {1.0f, 1.0f, 1.0f};
  glm::vec2 uvMultiplier  = {1.0f, 1.0f};
  glm::vec2 size          = {1.0f, 1.0f};
  BillboardType billboard = BillboardType::DISABLED;

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets      = {VK_NULL_HANDLE};
  FArray<vk::Buffer,      MAX_FRAMES_IN_FLIGHT> spriteDataUBO       = {{}};
  FArray<void*,           MAX_FRAMES_IN_FLIGHT> spriteDataUBOMapped = {nullptr};

  // This is used when sorting the sprites for rendering...
  f32 distanceToPlayer = F32_MAX;
};

#endif //SNOWFLAKE_SPRITE_HPP
