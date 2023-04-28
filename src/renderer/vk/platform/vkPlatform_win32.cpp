#include "renderer/vk/vkPlatform.hpp"

#if defined(PLATFORM_WINDOWS)

// Platform Includes
#include "core/Logging.hpp"
#include "core/Platform.hpp"
#include "core/Window.hpp"
#include <vulkan/vulkan_win32.h>

b8 vk::CreateVkSurface(VkInstance instance, VkAllocationCallbacks* allocationCallbacks, VkSurfaceKHR* out_surface)
{
  return CreateVkSurface(Window::GetNativeHandle(), instance, allocationCallbacks, out_surface);
}

b8 vk::CreateVkSurface(void* windowHandle, VkInstance instance, VkAllocationCallbacks* allocationCallbacks, VkSurfaceKHR* out_surface)
{
  LOG_INFO("Creating Vulkan Surface (Platform: Windows)...");

  HINSTANCE* hinstance = (HINSTANCE*)Platform::GetNativeInstance();
  HWND* hwnd           = (HWND*)windowHandle;

  if (hwnd == nullptr)
  {
    LOG_FATAL("Window handle is null!");
    return false;
  }

  VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  create_info.hinstance = *hinstance;
  create_info.hwnd      = *hwnd;

  VkResult result = vkCreateWin32SurfaceKHR(instance, &create_info, allocationCallbacks, out_surface);
  if (result != VK_SUCCESS)
  {
    LOG_FATAL("Windows: Vulkan surface creation failed.");
    return false;
  }

  LOG_INFO("Created Vulkan Surface (Platform: Windows)!");
  return true;
}

#endif