#ifndef SNOWFLAKE_MATERIAL_HPP
#define SNOWFLAKE_MATERIAL_HPP

#include "pch.hpp"
#include "containers/FArray.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/Texture2D.hpp"

struct Material
{
  Texture2D albedo;

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {VK_NULL_HANDLE};

  static Material* CreateMaterial(const char* albedoTexPath, glm::vec3 colour = {1.0f, 1.0f, 1.0f});
  static void DestroyMaterial(Material* material);

  static void InitMaterialSystem();
  static void ShutdownMaterialSystem();

  static inline VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

  static inline Material* defaultMaterial = nullptr;

  struct PushConstants
  {
    alignas(16) glm::vec3 colour;
    alignas(8)  glm::vec2 texTiling;
  };
};

#endif //SNOWFLAKE_MATERIAL_HPP
