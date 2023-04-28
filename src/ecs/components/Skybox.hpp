#ifndef SNOWFLAKE_SKYBOX_HPP
#define SNOWFLAKE_SKYBOX_HPP

#include "pch.hpp"
#include "ecs/components/MeshRenderer.hpp"

// Forward Declarations
struct SkyboxCreateInfo;

struct Skybox
{
  glm::vec3 colour = {1.0f, 1.0f, 1.0f};

  vk::Image textureImage       = {};
  VkImageView textureImageView = {};
  VkSampler textureSampler     = {};

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets      = {VK_NULL_HANDLE};
  FArray<vk::Buffer,      MAX_FRAMES_IN_FLIGHT> skyboxDataUBO       = {{}};
  FArray<void*,           MAX_FRAMES_IN_FLIGHT> skyboxDataUBOMapped = {nullptr};
};

#endif //SNOWFLAKE_SKYBOX_HPP
