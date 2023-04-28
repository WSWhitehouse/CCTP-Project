#include "renderer/Texture2D.hpp"

// core
#include "core/Assert.hpp"

// renderer
#include "renderer/Renderer.hpp"

// filesystem
#include "filesystem/AssetDatabase.hpp"

b8 Texture2D::CreateImageFromRawData(const TextureData* textureData)
{
  const vk::Device& device = Renderer::GetDevice();

  // Create VkImage...
  {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.extent      =
      {
        .width  = textureData->width,
        .height = textureData->height,
        .depth  = 1
      };
    imageCreateInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags         = 0;

    if (!image.Create(device, &imageCreateInfo))
    {
      LOG_ERROR("Texture2D Image creation failed!");
      return false;
    }
  }

  // Copy texture data to image
  {
    const VkDeviceSize imageSize = (VkDeviceSize)((u64)textureData->width * (u64)textureData->height * 4);

    vk::Buffer stagingBuffer;
    stagingBuffer.Create(device, imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    byte* data;
    stagingBuffer.MapMemory(device, (void**)&data);
    mem_copy(data, textureData->pixels, imageSize);
    stagingBuffer.UnmapMemory(device);

    vk::Image::TransitionLayout(image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk::Buffer::CopyBufferToImage(stagingBuffer, image.image, textureData->width, textureData->height, 1);
    vk::Image::TransitionLayout(image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    stagingBuffer.Destroy(device);
  }

  // Create image view
  {
    VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image            = image.image;
    createInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    createInfo.format           = VK_FORMAT_R8G8B8A8_SRGB;
    createInfo.subresourceRange =
      {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
      };

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &createInfo, nullptr, &imageView));
  }

  return true;
}

b8 Texture2D::CreateImageArrayFromRawData(TextureData* const* textureData, u32 textureCount)
{
  ASSERT_MSG(textureData != nullptr, "Texture data is nullptr! Please pass a valid array!");
  ASSERT_MSG(textureCount != 0, "Texture data count is 0!");

  const vk::Device& device = Renderer::GetDevice();

  const u32 width  = textureData[0]->width;
  const u32 height = textureData[0]->height;

#if defined(_DEBUG)
  for (u32 i = 1; i < textureCount; ++i)
  {
    ASSERT_MSG(textureData[i]->width  == width,  "All textures in the array must be of the same Width!")
    ASSERT_MSG(textureData[i]->height == height, "All textures in the array must be of the same Height!")
  }
#endif

  // Create VkImage...
  {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = textureCount;
    imageCreateInfo.extent      =
      {
        .width  = width,
        .height = height,
        .depth  = 1
      };
    imageCreateInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags         = 0;

    if (!image.Create(device, &imageCreateInfo))
    {
      LOG_ERROR("Texture2D Image creation failed!");
      return false;
    }
  }

  // Copy texture data to image
  {
    const VkDeviceSize imageSize = (VkDeviceSize)((u64)width * (u64)height * 4);
    const VkDeviceSize totalSize = imageSize * textureCount;

    vk::Buffer stagingBuffer;
    stagingBuffer.Create(device, totalSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    byte* data;
    stagingBuffer.MapMemory(device, (void**)&data);

    for (u32 i = 0; i < textureCount; ++i)
    {
      mem_copy(data, textureData[i]->pixels, imageSize);
      data += imageSize;
    }

    stagingBuffer.UnmapMemory(device);

    vk::Image::TransitionLayout(image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textureCount);
    vk::Buffer::CopyBufferToImageArray(stagingBuffer, image.image, width, height, 1, 0, textureCount);
    vk::Image::TransitionLayout(image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, textureCount);

    stagingBuffer.Destroy(device);
  }

  // Create image view
  {
    VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image            = image.image;
    createInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    createInfo.format           = VK_FORMAT_R8G8B8A8_SRGB;
    createInfo.subresourceRange =
      {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = textureCount
      };

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &createInfo, nullptr, &imageView));
  }

  return true;
}

b8 Texture2D::CreateSampler(SamplerFilter samplerFilter, VkSamplerAddressMode uvAddressMode)
{
  const vk::Device& device = Renderer::GetDevice();

  VkFilter filter = samplerFilter == SamplerFilter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

  VkSamplerCreateInfo createInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  createInfo.magFilter = filter;
  createInfo.minFilter = filter;

  createInfo.addressModeU = uvAddressMode;
  createInfo.addressModeV = uvAddressMode;
  createInfo.addressModeW = uvAddressMode;

  createInfo.anisotropyEnable        = VK_TRUE;
  createInfo.maxAnisotropy           = device.properties.limits.maxSamplerAnisotropy;
  createInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  createInfo.unnormalizedCoordinates = VK_FALSE;
  createInfo.compareEnable           = VK_FALSE;
  createInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
  createInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  createInfo.mipLodBias              = 0.0f;
  createInfo.minLod                  = 0.0f;
  createInfo.maxLod                  = 0.0f;

  VK_SUCCESS_CHECK(vkCreateSampler(device.logicalDevice, &createInfo, nullptr, &sampler));

  return true;
}

void Texture2D::DestroyImage()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyImageView(device.logicalDevice, imageView, nullptr);
  image.Destroy(device);

  imageView = VK_NULL_HANDLE;
}

void Texture2D::DestroySampler()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroySampler(device.logicalDevice, sampler, nullptr);

  sampler = VK_NULL_HANDLE;
}