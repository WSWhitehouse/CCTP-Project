#include "renderer/vk/Image.hpp"

#include "renderer/vk/Vulkan.hpp"
#include "renderer/Renderer.hpp"

using namespace vk;

b8 Image::Create(const Device& device, const VkImageCreateInfo* imageCreateInfo)
{
  // Create image
  VK_SUCCESS_CHECK(vkCreateImage(device.logicalDevice, imageCreateInfo, nullptr, &image));

  // Allocate and bind image memory
  {
    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(device.logicalDevice, image, &memoryRequirements);

    u32 memType;
    if (!FindSupportedMemoryType(device,
                                 memoryRequirements.memoryTypeBits,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 &memType))
    {
      LOG_ERROR("Failed to find memory type when creating memory!");
      return false;
    }

    VkMemoryAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize  = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = memType;

    VK_SUCCESS_CHECK(vkAllocateMemory(device.logicalDevice, &allocateInfo, nullptr, &imageMemory));
    VK_SUCCESS_CHECK(vkBindImageMemory(device.logicalDevice, image, imageMemory, 0));
  }

  return true;
}

void Image::Destroy(const Device& device)
{
  // Vulkan API calls...
  vkDestroyImage(device.logicalDevice, image, nullptr);
  vkFreeMemory(device.logicalDevice, imageMemory, nullptr);

  // Reset image data
  image       = VK_NULL_HANDLE;
  imageMemory = VK_NULL_HANDLE;
}

void Image::TransitionLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, u32 layerCount)
{
  const vk::Device& device       = Renderer::GetDevice();
  const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();

  VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

  VkImageSubresourceRange subresourceRange =
    {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = layerCount
    };

  CmdTransitionBarrier(cmdBuffer, image, oldLayout, newLayout, subresourceRange);

  cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
}

void Image::CmdTransitionBarrier(VkCommandBuffer cmdBuffer, VkImage image,
                                 VkImageLayout oldLayout, VkImageLayout newLayout,
                                 VkImageSubresourceRange subresourceRange,
                                 u32 srcQueueFamilyIndex, u32 dstQueueFamilyIndex)
{
  VkImageMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  memoryBarrier.oldLayout           = oldLayout;
  memoryBarrier.newLayout           = newLayout;
  memoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
  memoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
  memoryBarrier.image               = image;
  memoryBarrier.subresourceRange    = subresourceRange;

  VkPipelineStageFlags srcStage, dstStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
  {
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
  {
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
  {
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
  {
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
  {
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else
  {
    LOG_FATAL("Unsupported layout transition!");
    return;
  }

  vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}