#include "renderer/vk/Buffer.hpp"

#include "renderer/vk/Vulkan.hpp"
#include "renderer/Renderer.hpp"

using namespace vk;

b8 Buffer::Create(const Device& device, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
  // Set buffer data
  this->size = bufferSize;

  // Create buffer
  {
    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size        = bufferSize;
    bufferCreateInfo.usage       = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // REVIEW(WSWhitehouse): Should this be a param?

    VK_SUCCESS_CHECK(vkCreateBuffer(device.logicalDevice, &bufferCreateInfo, nullptr, &buffer));
  }

  // Allocate buffer memory and bind
  {
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device.logicalDevice, buffer, &memoryRequirements);

    u32 memoryType;
    if (!FindSupportedMemoryType(device,
                                 memoryRequirements.memoryTypeBits,
                                 properties,
                                 &memoryType))
    {
      return false;
    }

    // REVIEW(WSWhitehouse): Should we always allocate and bind the buffer memory? Optional param?
    VkMemoryAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize  = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = memoryType;

    VK_SUCCESS_CHECK(vkAllocateMemory(device.logicalDevice, &allocateInfo, nullptr, &memory));
    vkBindBufferMemory(device.logicalDevice, buffer, memory, 0);
  }

  return true;
}

void Buffer::Destroy(const Device& device)
{
  // Vulkan API calls...
  vkDestroyBuffer(device.logicalDevice, buffer, nullptr);
  vkFreeMemory(device.logicalDevice, memory, nullptr);

  // Reset buffer data
  size   = 0;
  buffer = VK_NULL_HANDLE;
  memory = VK_NULL_HANDLE;
}

void Buffer::MapMemory(const Device& device, void** mappedMemory, VkDeviceSize mappedSize) const
{
  // REVIEW(WSWhitehouse): Should probably add offset and flags to params?
  VK_SUCCESS_CHECK(vkMapMemory(device.logicalDevice, memory, 0, mappedSize, 0, mappedMemory));
}

void Buffer::UnmapMemory(const Device& device) const
{
  vkUnmapMemory(device.logicalDevice, memory);
}

//void Buffer::CopyDataToBuffer(const Renderer* renderer,
//                              void* srcData, Buffer& dstBuffer, VkDeviceSize size)
//{
//  void* memory;
//  dstBuffer.MapMemory(renderer, &memory);
//  mem_copy(memory, srcData, size);
//  dstBuffer.UnmapMemory(renderer);
//}

void Buffer::CopyBufferToBuffer(Buffer& srcBuffer, Buffer& dstBuffer, VkDeviceSize size)
{
  const vk::Device& device       = Renderer::GetDevice();
  const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();

  VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

  VkBufferCopy bufferCopy = {};
  bufferCopy.size = size;
  vkCmdCopyBuffer(cmdBuffer, srcBuffer.buffer, dstBuffer.buffer, 1, &bufferCopy);

  cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
}

void Buffer::CopyBufferToImage(Buffer& srcBuffer, VkImage dstImage, u32 width, u32 height, u32 depth, VkImageAspectFlags aspectMask)
{
  const vk::Device& device       = Renderer::GetDevice();
  const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();

  VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

  VkBufferImageCopy copy = {};
  copy.bufferOffset      = 0;
  copy.bufferRowLength   = 0;
  copy.bufferImageHeight = 0;

  copy.imageSubresource =
    {
      .aspectMask     = aspectMask,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = 1
    };

  const VkOffset3D offset = {0, 0, 0};
  copy.imageOffset = offset;

  copy.imageExtent =
    {
      .width  = width,
      .height = height,
      .depth  = depth
    };

  vkCmdCopyBufferToImage(cmdBuffer, srcBuffer.buffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
}

void Buffer::CopyBufferToImageArray(Buffer& srcBuffer, VkImage dstImage,
                                    u32 width, u32 height, u32 depth,
                                    u32 baseArrayLayer, u32 layerCount,
                                    VkImageAspectFlags aspectMask)
{
  const vk::Device& device       = Renderer::GetDevice();
  const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();

  VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

  VkBufferImageCopy copy = {};
  copy.bufferOffset      = 0;
  copy.bufferRowLength   = 0;
  copy.bufferImageHeight = 0;

  copy.imageSubresource =
    {
      .aspectMask     = aspectMask,
      .mipLevel       = 0,
      .baseArrayLayer = baseArrayLayer,
      .layerCount     = layerCount
    };

  const VkOffset3D offset = {0, 0, 0};
  copy.imageOffset = offset;

  copy.imageExtent =
    {
      .width  = width,
      .height = height,
      .depth  = depth
    };

  vkCmdCopyBufferToImage(cmdBuffer, srcBuffer.buffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
}
