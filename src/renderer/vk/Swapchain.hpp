#ifndef SNOWFLAKE_VULKAN_TYPES_HPP
#define SNOWFLAKE_VULKAN_TYPES_HPP

#include "pch.hpp"
#include "Image.hpp"
#include <vulkan/vulkan.h>

namespace vk
{
  struct Swapchain
  {
    VkSwapchainKHR swapchain;

    VkFormat imageFormat;
    VkExtent2D extent;

    // NOTE(WSWhitehouse): This is the count of the images, depthImages and framebuffers...
    u32 imagesCount;

    VkImage* images;
    VkImageView* imageViews;

    vk::Image* depthImages;
    VkImageView* depthImageViews;
    VkFormat depthFormat;

    VkFramebuffer* framebuffers;
  };

  struct SwapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR capabilities;

    VkSurfaceFormatKHR* formats;
    u32 formatsCount;

    VkPresentModeKHR* presentModes;
    u32 presentModesCount;
  };

} // namespace vk

#endif //SNOWFLAKE_VULKAN_TYPES_HPP
