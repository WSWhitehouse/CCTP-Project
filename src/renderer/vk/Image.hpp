#ifndef SNOWFLAKE_IMAGE_HPP
#define SNOWFLAKE_IMAGE_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

namespace vk
{
  // Forward Declarations
  struct Device;

  /**
  * @brief This is a wrapper around the VkImage object. It handles image
  * creation along with memory allocation and binding. This does *NOT*
  * handle image views or samplers - they must be handled externally.
  */
  class Image
  {
  public:
    Image()  = default;
    ~Image() = default;
    DEFAULT_CLASS_COPY(Image);

    /**
    * @brief CreateECS image object then allocates and binds device memory.
    * @param device Device to be used in image creation.
    * @param imageCreateInfo The struct to be used during image creation.
    * @return True on success; false otherwise.
    */
    b8 Create(const Device& device, const VkImageCreateInfo* imageCreateInfo);

    /**
    * @brief Destroy the image object and free its memory.
    * @param device Device to be used in image destruction.
    */
    void Destroy(const Device& device);

    // --- Static Functions --- //

    /**
    * @brief Transitions an image layout from the oldLayout to newLayout.
    * @param image Image to transition layout.
    * @param oldLayout Layout to transition from.
    * @param newLayout Layout to transition to.
    * @param layerCount Number of image layers to transition (default: 1).
    */
    static void TransitionLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, u32 layerCount = 1);

    static void CmdTransitionBarrier(VkCommandBuffer cmdBuffer, VkImage image,
                                     VkImageLayout oldLayout, VkImageLayout newLayout,
                                     VkImageSubresourceRange subresourceRange,
                                     u32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     u32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

    // --- Image Member Data --- //
    VkImage image              = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
  };

} // namespace vk

#endif //SNOWFLAKE_IMAGE_HPP
