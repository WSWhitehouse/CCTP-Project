#ifndef SNOWFLAKE_SDF_VOXEL_GRID_HPP
#define SNOWFLAKE_SDF_VOXEL_GRID_HPP

#include "pch.hpp"
#include "renderer/vk/Vulkan.hpp"

// containers
#include "containers/FArray.hpp"

// Forward Declarations
struct Mesh;

struct SdfVoxelGrid
{
  static b8 CreateComputePipeline();
  static void CleanUpComputePipeline();

  static void Create(SdfVoxelGrid* voxelGrid, b8 useJumpFlooding, const Mesh* mesh, glm::uvec3 uCellCount);
  static void Release(SdfVoxelGrid* sdfVolume);

  // --- Member Data --- //

  /**
  * @brief The cell count for the 3D Image.
  * w = x * y * z
  */
  glm::uvec4 cellCount = { 0, 0, 0, 0 };

  /**
  * @brief The extents of the voxel grid.
  */
  glm::vec3 gridExtent = { 0.0f, 0.0f, 0.0f };

  /**
  * @brief The mesh geometry might not be centered, this is the offset
  * applied in the voxel generation compute shader. This is to ensure
  * the mesh isn't cut off and the voxels are used efficiently.
  */
  glm::vec3 gridCenterOffset = { 0.0f, 0.0f, 0.0f };

  /**
  * @brief The size of one cell in the grid.
  */
  glm::vec3 cellSize = { 0.0f, 0.0f, 0.0f };

  /**
  * @brief The scaling factor used to scale the mesh to the voxel grid.
  * Calculated as the minimum value in the scalingFactor vector.
  */
  f32 scalingFactor = 0.0f;

  glm::vec3 twist;
  b8 showBounds = false;

  glm::vec4 sphere = {0.0f, 0.5f, 5.0f, 0.1f};
  float sphereBlend = 0.1f;

  vk::Image image        = {};
  VkImageView imageView  = VK_NULL_HANDLE;
  VkSampler imageSampler = VK_NULL_HANDLE;

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets           = {VK_NULL_HANDLE};
  FArray<vk::Buffer,      MAX_FRAMES_IN_FLIGHT> dataUniformBuffer        = {{}};
  FArray<void*,           MAX_FRAMES_IN_FLIGHT> dataUniformBuffersMapped = {nullptr};
};

#endif //SNOWFLAKE_SDF_VOXEL_GRID_HPP
