#ifndef SNOWFLAKE_SDF_POINT_CLOUD_RENDERER_HPP
#define SNOWFLAKE_SDF_POINT_CLOUD_RENDERER_HPP

#include "pch.hpp"

// containers
#include "containers/DArray.hpp"
#include "containers/FArray.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"

// geometry
#include "geometry/Plane.hpp"
#include "geometry/BoundingBox3D.hpp"

struct SdfPointCloudRenderer
{
  u32 pointCount = 0;

  struct BSPNodeUBO
  {
    alignas(16) Plane plane = {};
    alignas(16) glm::vec3 boundingBox = {};

    alignas(4) u32 isLeaf = 0;

    // NOTE(WSWhitehouse): Should only link to other nodes if this is not a leaf
    alignas(4) u32 nodePositive = U32_MAX;
    alignas(4) u32 nodeNegative = U32_MAX;

    alignas(4) u32 pointIndex;
    alignas(4) u32 pointCount;
  };

  DArray<BSPNodeUBO> nodes = {};
  BoundingBox3D boundingBox = {};

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets = {VK_NULL_HANDLE};

  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> dataUBO       = {{}};
  FArray<void*,      MAX_FRAMES_IN_FLIGHT> dataUBOMapped = {nullptr};

  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> bspNodesUBO = {{}};
  FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT> pointsUBO   = {{}};
};

#endif // SNOWFLAKE_SDF_POINT_CLOUD_RENDERER_HPP

