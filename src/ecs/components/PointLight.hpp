#ifndef SNOWFLAKE_POINT_LIGHT_HPP
#define SNOWFLAKE_POINT_LIGHT_HPP

#include "pch.hpp"

#define MAX_POINT_LIGHT_COUNT 1

struct PointLight
{
  glm::vec3 colour = { 1.0f, 1.0f, 1.0f };
  f32 range        = 1.0f;
};

// NOTE(WSWhitehouse): This component is used directly in the UBO (see
// UniformBufferObjects.hpp). Variables are aligned ready for the GPU.
struct RawPointLight
{
  alignas(16) glm::vec3 position = { 0.0f, 0.0f, 0.0f };
  alignas(16) glm::vec3 colour   = { 1.0f, 1.0f, 1.0f };
  alignas(04) f32 range          = 1.0f;
};

#endif //SNOWFLAKE_POINT_LIGHT_HPP
