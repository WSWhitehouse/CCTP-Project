#ifndef SNOWFLAKE_UI_IMAGE_HPP
#define SNOWFLAKE_UI_IMAGE_HPP

#include "pch.hpp"
#include "renderer/Texture2D.hpp"

struct UIImage
{
  b8 render = true;

  Texture2D texture;
  u32 textureCount    = 0;
  u32 currentTexIndex = 0;

  glm::vec3 colour = {1.0f, 1.0f, 1.0f};

  glm::vec2 pos;
  glm::vec2 size;
  glm::vec2 scale = glm::vec2(1.0f, 1.0f);
  f32 zOrder = 0.0f; // between 0.0f - 1.0f

  VkDescriptorSet descriptorSets = VK_NULL_HANDLE;
};

#endif //SNOWFLAKE_UI_IMAGE_HPP
