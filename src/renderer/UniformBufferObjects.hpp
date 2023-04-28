#ifndef SNOWFLAKE_SHADER_CORE_HPP
#define SNOWFLAKE_SHADER_CORE_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>
#include "core/Assert.hpp"

// ecs
#include "ecs/components/PointLight.hpp"

// NOTE(WSWhitehouse):
// This file contains all the UBO structs. The fields in these structs must be aligned properly
// for the GPU (format: std140). This means fields are organised in a way to use space efficiently
// rather than what makes sense... Here is a useful table of alignments:
//
//  Taken from: https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL
// -------------------------------------------------------------------------
//   TYPE     |  BYTE ALIGNMENT
// -------------------------------------------------------------------------
//  Scalar    |  4
//            |
//  Vector2   |  8
//            |
//  Vector3/4 |  16
//            |
//  Array     |  Each element has a base alignment equal to 16.
//            |
//  Matrix    |  Stored as an array of column vectors, each aligned to 16.
//            |
//  Struct    |  Equal to the computed size of its elements according to
//            |  prev rules, but padded to a multiple of 16.
// -------------------------------------------------------------------------

/**
 * @brief Application and frame related data. Updated once per-frame.
 * layout(binding = 0)
 */
struct UBOFrameData
{
  static inline const i32 Binding = 0;

  // --- Uniform Data --- //
  alignas(16) RawPointLight pointLights[MAX_POINT_LIGHT_COUNT];
  alignas(04) i32 pointLightCount;
  alignas(16) glm::vec3 ambientColour;
  alignas(04) f32 time;
  alignas(04) f32 sinTime;

  // --- Utility Functions --- //
  static INLINE VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding()
  {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding            = Binding;
    layoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount    = 1;
    layoutBinding.pImmutableSamplers = nullptr;
    layoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    return layoutBinding;
  }

  static INLINE VkDescriptorBufferInfo GetDescriptorBufferInfo(VkBuffer buffer)
  {
    VkDescriptorBufferInfo bufferInfo = {};
    mem_zero(&bufferInfo, sizeof(VkDescriptorBufferInfo));
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UBOFrameData);
    return bufferInfo;
  }

  static INLINE VkWriteDescriptorSet GetWriteDescriptorSet(VkDescriptorSet descriptorSet, const VkDescriptorBufferInfo* bufferInfo)
  {
    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = Binding;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.pBufferInfo      = bufferInfo;
    descriptorWrite.pImageInfo       = nullptr;
    descriptorWrite.pTexelBufferView = nullptr;
    return descriptorWrite;
  }
};

STATIC_ASSERT(sizeof(UBOFrameData) % 16 == 0);

/**
 * @brief Camera related data. Updated per-camera.
 * layout(binding = 1)
 */
struct UBOCameraData
{
  static inline const i32 Binding = 1;

  // --- Uniform Data --- //
  alignas(16) glm::vec3 position;
  alignas(16) glm::mat4 viewMat;
  alignas(16) glm::mat4 projMat;
  alignas(16) glm::mat4 invViewMat;
  alignas(16) glm::mat4 invProjMat;

  // --- Utility Functions --- //
  static INLINE VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding()
  {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding            = Binding;
    layoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount    = 1;
    layoutBinding.pImmutableSamplers = nullptr;
    layoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    return layoutBinding;
  }

  static INLINE VkDescriptorBufferInfo GetDescriptorBufferInfo(VkBuffer buffer)
  {
    VkDescriptorBufferInfo bufferInfo = {};
    mem_zero(&bufferInfo, sizeof(VkDescriptorBufferInfo));
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UBOCameraData);
    return bufferInfo;
  }

  static INLINE VkWriteDescriptorSet GetWriteDescriptorSet(VkDescriptorSet descriptorSet, const VkDescriptorBufferInfo* bufferInfo)
  {
    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet           = descriptorSet;
    descriptorWrite.dstBinding       = Binding;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.pBufferInfo      = bufferInfo;
    descriptorWrite.pImageInfo       = nullptr;
    descriptorWrite.pTexelBufferView = nullptr;
    return descriptorWrite;
  }
};

STATIC_ASSERT(sizeof(UBOCameraData) % 16 == 0);

#endif //SNOWFLAKE_SHADER_CORE_HPP
