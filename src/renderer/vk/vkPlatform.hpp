#ifndef SNOWFLAKE_VK_PLATFORM_HPP
#define SNOWFLAKE_VK_PLATFORM_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

// NOTE(WSWhitehouse):
// Platform utility functions for using Vulkan, specific implementations are found
// in the platform folder. With the platform as a suffix on the file name - renderer
// should not be using platform specific code.

namespace vk
{
  /**
  * @brief Creates a platform specific VkSurface. Uses the main window handle.
  * @param instance The VkInstance used to create the VkSurface.
  * @param allocationCallbacks The vulkan allocation callback.
  * @param out_surface The pointer to the VkSurface that is created. Can NOT be nullptr.
  * @return True on successfully creating a VkSurface; false otherwise.
  */
  b8 CreateVkSurface(VkInstance instance, VkAllocationCallbacks* allocationCallbacks, VkSurfaceKHR* out_surface);

  /**
  * @brief Creates a platform specific VkSurface. Can specify specific window handle to
  * use during VkSurface creation.
  * @param windowHandle The window handle to use for creating the VkSurface.
  * @param instance The VkInstance used to create the VkSurface.
  * @param allocationCallbacks The vulkan allocation callback.
  * @param out_surface The pointer to the VkSurface that is created. Can NOT be nullptr.
  * @return True on successfully creating a VkSurface; false otherwise.
  */
  b8 CreateVkSurface(void* windowHandle, VkInstance instance, VkAllocationCallbacks* allocationCallbacks, VkSurfaceKHR* out_surface);

} // namespace vk

#endif //SNOWFLAKE_VK_PLATFORM_HPP
