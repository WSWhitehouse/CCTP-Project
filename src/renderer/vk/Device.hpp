#ifndef SNOWFLAKE_DEVICE_HPP
#define SNOWFLAKE_DEVICE_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

namespace vk
{

  struct QueueFamilyIndices
  {
    u32 graphicsFamily;
    b8 graphicsFound;

    u32 presentFamily;
    b8 presentFound;

    u32 computeFamily;
    b8 computeFound;
  };

  struct Device
  {
    // Physical Device
    VkPhysicalDevice physicalDevice;
    VkSampleCountFlagBits msaaSampleCount;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;

    QueueFamilyIndices queueFamilyIndices;

    // Logical Device
    VkDevice logicalDevice;

    // Device Queues
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
  };

} // namespace vk

#endif //SNOWFLAKE_DEVICE_HPP
