#ifndef SNOWFLAKE_BUFFER_HPP
#define SNOWFLAKE_BUFFER_HPP

#include "pch.hpp"
#include <vulkan/vulkan.h>

namespace vk
{
  // Forward Declarations
  struct Device;

  /**
  * @brief This class is a wrapper around the VkBuffer object. It handles
  * creation/destruction and memory mapping. This class handles allocating
  * and binding memory for the buffer in its create function. Also,
  * holds Size information for later use.
  */
  class Buffer
  {
  public:
    Buffer()  = default;
    ~Buffer() = default;
    DEFAULT_CLASS_COPY(Buffer);

    /**
    * @brief CreateECS a buffer and memory with the following parameters.
    * @param device Device used to create the buffer.
    * @param bufferSize Size of buffer to create.
    * @param usage Usage flags used when creating the buffer.
    * @param properties Property flags used when creating the buffer.
    * @return True on success, false on failure.
    */
    b8 Create(const Device& device, VkDeviceSize bufferSize,
              VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    /**
    * @brief Destroy the buffer and its memory.
    * @param device Device used to destroy the buffer.
    */
    void Destroy(const Device& device);

    // --- Memory Mapping Functions --- //
    void MapMemory(const Device& device, void** mappedMemory, VkDeviceSize mappedSize = VK_WHOLE_SIZE) const;
    void UnmapMemory(const Device& device) const;

    // --- Copying Functions --- //
  //  static void CopyDataToBuffer(const Renderer* renderer,
  //                               void* srcData, Buffer& dstBuffer, VkDeviceSize size);

    static void CopyBufferToBuffer(Buffer& srcBuffer, Buffer& dstBuffer,
                                   VkDeviceSize size = VK_WHOLE_SIZE);

    static void CopyBufferToImage(Buffer& srcBuffer, VkImage dstImage, u32 width, u32 height, u32 depth,
                                  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    static void CopyBufferToImageArray(Buffer& srcBuffer, VkImage dstImage,
                                       u32 width, u32 height, u32 depth,
                                       u32 baseArrayLayer, u32 layerCount,
                                       VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    // --- Buffer Member Data --- //
    VkDeviceSize size     = 0;
    VkBuffer buffer       = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
  };

} // namespace vk

#endif //SNOWFLAKE_BUFFER_HPP
