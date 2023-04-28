#ifndef SNOWFLAKE_VK_EXTENSIONS_INL
#define SNOWFLAKE_VK_EXTENSIONS_INL

#include "pch.hpp"
#include "renderer/vk/Vulkan.hpp"

static constexpr const char* instanceExtensions[]
{
  VK_KHR_SURFACE_EXTENSION_NAME,

#if defined(VK_VALIDATION)
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif

  // Platform Specific Extensions...

#if defined(PLATFORM_WINDOWS)
  "VK_KHR_win32_surface"
#endif
};

static constexpr const char* deviceExtensions[]
{
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
  VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
//  VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME
};

#endif //SNOWFLAKE_VK_EXTENSIONS_INL
