#ifndef SNOWFLAKE_VK_VALIDATION_INL
#define SNOWFLAKE_VK_VALIDATION_INL

#include "pch.hpp"
#include "renderer/vk/Vulkan.hpp"

#if defined(VK_VALIDATION)
  static const char* validationLayers[] =
    {
      "VK_LAYER_KHRONOS_validation",
      // "VK_LAYER_LUNARG_api_dump"
    };

  static VkDebugUtilsMessengerEXT vkValidationMessengerHandle = VK_NULL_HANDLE;

  static VKAPI_ATTR VkBool32 VKAPI_CALL vkValidationMessenger(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*                                       userData)
  {
    UNUSED(messageTypes);
    UNUSED(userData);

    switch (messageSeverity)
    {
      default:
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      {
        LOG_ERROR(callbackData->pMessage);
        break;
      }

      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      {
        LOG_WARN(callbackData->pMessage);
        break;
      }

      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      {
        LOG_INFO(callbackData->pMessage);
        break;
      }
    }

    return VK_FALSE;
  }
#endif

#endif //SNOWFLAKE_VK_VALIDATION_INL
