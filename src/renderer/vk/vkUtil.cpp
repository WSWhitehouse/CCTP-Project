#include "renderer/vk/vkUtil.hpp"
#include "filesystem/FileSystem.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "core/Assert.hpp"

using namespace vk;

VkShaderModule vk::CreateShaderModule(const Device& device, FileSystem::FileContent shaderCode, const VkAllocationCallbacks* pAllocator)
{
  VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  createInfo.codeSize = shaderCode.size;
  createInfo.pCode    = (u32*)shaderCode.data;

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  VK_SUCCESS_CHECK(vkCreateShaderModule(device.logicalDevice, &createInfo, pAllocator, &shaderModule));
  return shaderModule;
}

b8 vk::FindSupportedMemoryType(const Device& device, u32 typeFilter, VkMemoryPropertyFlags properties, u32* out_type)
{
  ASSERT_MSG(out_type != nullptr, "FindSupportedMemoryType: Out Type is nullptr! Please pass a valid out pointer.")

  for (u32 i = 0; i < device.memory.memoryTypeCount; i++)
  {
    if (BITFLAG_HAS_FLAG(typeFilter, (1 << i)) &&
        BITFLAG_HAS_ALL(device.memory.memoryTypes[i].propertyFlags, properties))
    {
      (*out_type) = i;
      return true;
    }
  }

  return false;
}

b8 vk::FindSupportedFormat(const Device& device, const VkFormat* formatCandidates, u32 candidateCount,
                           VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat* out_format)
{
  ASSERT_MSG(out_format != nullptr, "FindSupportedFormat: Out Format is nullptr! Please pass a valid out pointer.")

  for (u32 i = 0; i < candidateCount; ++i)
  {
    const VkFormat& format = formatCandidates[i];

    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device.physicalDevice, format, &properties);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        BITFLAG_HAS_ALL(properties.linearTilingFeatures, features))
    {
      (*out_format) = format;
      return true;
    }

    if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        BITFLAG_HAS_ALL(properties.optimalTilingFeatures, features))
    {
      (*out_format) = format;
      return true;
    }
  }

  return false;
}