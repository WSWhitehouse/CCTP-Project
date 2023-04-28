#ifndef SNOWFLAKE_SDF_RENDERER_HPP
#define SNOWFLAKE_SDF_RENDERER_HPP

#include "pch.hpp"

// containers
#include "containers/FArray.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"

// geometry
#include "geometry/BoundingBox3D.hpp"

struct SdfRenderer
{
  BoundingBox3D boundingBox = {};
  u32 indexCount = 0;

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {VK_NULL_HANDLE};

  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> dataUBO       = {{}};
  FArray<void*,      MAX_FRAMES_IN_FLIGHT> dataUBOMapped = {nullptr};

  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> bspNodesUBO = {{}};
  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> indicesUBO  = {{}};
  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> verticesUBO = {{}};
};

#endif //SNOWFLAKE_SDF_RENDERER_HPP
