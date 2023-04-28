#include "renderer/vk/CommandPool.hpp"
#include "renderer/vk/Vulkan.hpp"

using namespace vk;

b8 CommandPool::Create(const vk::Device& device, u32 queueFamilyIndex, VkQueue queue,
                       VkCommandPoolCreateFlagBits createFlags)
{
  // Set member data
  {
    this->createFlags      = createFlags;
    this->queueFamilyIndex = queueFamilyIndex;
    this->queue            = queue;
  }

  // Create command pool
  {
    VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags            = createFlags;
    poolCreateInfo.queueFamilyIndex = this->queueFamilyIndex;

    VK_SUCCESS_CHECK(vkCreateCommandPool(device.logicalDevice, &poolCreateInfo, nullptr, &pool));
  }

  return true;
}

void CommandPool::Destroy(const vk::Device& device)
{
  // Vulkan API calls...
  vkDestroyCommandPool(device.logicalDevice, pool, nullptr);

  // Reset data
  pool             = VK_NULL_HANDLE;
  queueFamilyIndex = 0;
  queue            = VK_NULL_HANDLE;
}

VkCommandBuffer CommandPool::SingleTimeCommandBegin(const Device& device) const
{
  VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandPool        = pool;
  allocateInfo.commandBufferCount = 1;

  VkCommandBuffer cmdBuffer;
  VK_SUCCESS_CHECK(vkAllocateCommandBuffers(device.logicalDevice, &allocateInfo, &cmdBuffer));

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_SUCCESS_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

  return cmdBuffer;
}

void CommandPool::SingleTimeCommandEnd(const Device& device, VkCommandBuffer cmdBuffer) const
{
  VK_SUCCESS_CHECK(vkEndCommandBuffer(cmdBuffer));

  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer;

  VK_SUCCESS_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  VkResult result = vkQueueWaitIdle(queue);

  VK_SUCCESS_CHECK(result);

  // NOTE(WSWhitehouse): I'm a little unsure why the command buffer needs to be reset here before we
  // destroy it on the line below. But on some hardware not resetting the command buffer causes
  // validation errors and a crash.
  if (createFlags != VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)
  {
    VK_SUCCESS_CHECK(vkResetCommandBuffer(cmdBuffer, 0));
  }

  vkFreeCommandBuffers(device.logicalDevice, pool, 1, &cmdBuffer);
}