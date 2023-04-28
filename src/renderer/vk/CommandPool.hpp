#ifndef SNOWFLAKE_COMMAND_POOL_HPP
#define SNOWFLAKE_COMMAND_POOL_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

namespace vk
{
  // Forward Declarations
  struct Device;

  class CommandPool
  {
  public:
    CommandPool()  = default;
    ~CommandPool() = default;
    DEFAULT_CLASS_COPY(CommandPool);

    b8 Create(const Device& device, u32 queueFamilyIndex, VkQueue queue,
              VkCommandPoolCreateFlagBits createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    void Destroy(const Device& device);

    // --- Single Time Command --- //

    [[nodiscard]] VkCommandBuffer SingleTimeCommandBegin(const Device& device) const;
    void SingleTimeCommandEnd(const Device& device, VkCommandBuffer cmdBuffer) const;

    // --- Command Pool Member Data --- //
    VkCommandPool pool = VK_NULL_HANDLE;

  private:
    // NOTE(WSWhitehouse): These variables should not be accessed outside of
    // the command pool. They are used for submitting single time commands.
    u32 queueFamilyIndex                    = 0;
    VkQueue queue                           = VK_NULL_HANDLE;
    VkCommandPoolCreateFlagBits createFlags = VK_COMMAND_POOL_CREATE_FLAG_BITS_MAX_ENUM;
  };

} // namespace vk

#endif //SNOWFLAKE_COMMAND_POOL_HPP
