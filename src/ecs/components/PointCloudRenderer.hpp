#ifndef SNOWFLAKE_POINT_CLOUD_RENDERER_HPP
#define SNOWFLAKE_POINT_CLOUD_RENDERER_HPP

#include "pch.hpp"

// containers
#include "containers/FArray.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"

struct PointCloudRenderer
{
  b8 render = true;

  vk::Buffer pointBuffer = {};
  u32 pointCount         = 0;

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets     = {VK_NULL_HANDLE};
  FArray<vk::Buffer,      MAX_FRAMES_IN_FLIGHT> modelDataUBO       = {{}};
  FArray<void*,           MAX_FRAMES_IN_FLIGHT> modelDataUBOMapped = {nullptr};
};

#endif //SNOWFLAKE_POINT_CLOUD_RENDERER_HPP
