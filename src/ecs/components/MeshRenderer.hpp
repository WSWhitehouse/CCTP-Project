#ifndef SNOWFLAKE_MESH_RENDERER_HPP
#define SNOWFLAKE_MESH_RENDERER_HPP

#include "pch.hpp"

// containers
#include "containers/FArray.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"

// Forward Declarations
struct Material;

struct MeshBufferData
{
  vk::Buffer vertexBuffer = {};
  vk::Buffer indexBuffer  = {};
  u64 indexCount          = 0;
  VkIndexType indexType   = VK_INDEX_TYPE_UINT16;
};

struct MeshRenderer
{
  // --- DATA --- //
  b8 renderMesh = true;

  // --- MESH BUFFERS --- //
  MeshBufferData* bufferDataArray = nullptr;
  u32 bufferDataCount             = 0;

  Material* material = nullptr;
  glm::vec3 colour;
  glm::vec2 texTiling;

  // --- UNIFORM BUFFERS & DESCRIPTOR SETS --- //
  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets     = {VK_NULL_HANDLE};
  FArray<vk::Buffer,      MAX_FRAMES_IN_FLIGHT> modelDataUBO       = {{}};
  FArray<void*,           MAX_FRAMES_IN_FLIGHT> modelDataUBOMapped = {nullptr};
};

#endif //SNOWFLAKE_MESH_RENDERER_HPP
