#ifndef SNOWFLAKE_VK_UTIL_HPP
#define SNOWFLAKE_VK_UTIL_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

// NOTE(WSWhitehouse):
// This file contains useful static Vulkan utility functions that are used
// throughout the renderer and dont fit into a specific file.

// Forward Declarations
namespace FileSystem { struct FileContent; }

namespace vk
{
  // Forward Declarations
  struct Device;

  /**
  * @brief Creates a shader module from file content. Please
  * note: This function doesn't handle freeing the file content.
  * @param device Device to be used during shader content creation.
  * @param shaderCode The file content to use as shader code.
  * @param pAllocator Vulkan allocator to use when creating the module.
  * @return The created Shader Module.
  */
  VkShaderModule CreateShaderModule(const Device& device, FileSystem::FileContent shaderCode, const VkAllocationCallbacks* pAllocator);

  /**
  * @brief Find the memory type based on the parameters.
  * @param device Device to use for finding the memory type.
  * @param typeFilter Type of memory to search for.
  * @param properties Properties of memory.
  * @param out_type Pointer to variable to hold found memory type. Must NOT be nullptr.
  * @return True when successfully finding a memory type; false otherwise.
  */
  b8 FindSupportedMemoryType(const Device& device, u32 typeFilter, VkMemoryPropertyFlags properties, u32* out_type);

  /**
  * @brief Find a supported format based on the parameters.
  * @param device Device to use for finding the supported format.
  * @param formatCandidates Array of format candidates to check. Ordered in most to least desired.
  * @param candidateCount Count of formatCandidates array.
  * @param tiling Tiling mode for format property.
  * @param features Required features of format.
  * @param out_format Pointer to variable to hold found format. Must NOT be nullptr.
  * @return True when successfully finding a supported format; false otherwise.
  */
  b8 FindSupportedFormat(const Device& device, const VkFormat* formatCandidates, u32 candidateCount,
                         VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat* out_format);

  /**
  * @brief Find a supported depth format.
  * @param device Device to use for finding the supported format.
  * @param out_format Pointer to format variable to hold found format. Must NOT be nullptr.
  * @return True when successfully finding a supported depth format; false otherwise.
  */
  INLINE b8 FindDepthFormat(const Device& device, VkFormat* out_format)
  {
    // NOTE(WSWhitehouse): Candidate Formats are ordered from most to least desirable...
    constexpr const VkFormat candidateFormats[]
      {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
      };

    return FindSupportedFormat(device,
                               candidateFormats,
                               ARRAY_SIZE(candidateFormats),
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               out_format);
  }

  /**
  * @brief Check if the depth format contains a stencil component.
  * @param format Format to check for stencil component.
  * @return True when format contains a stencil component; false otherwise.
  */
  INLINE b8 DepthFormatHasStencilComponent(VkFormat format)
  {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
  }

} // namespace vk

#endif //SNOWFLAKE_VK_UTIL_HPP
