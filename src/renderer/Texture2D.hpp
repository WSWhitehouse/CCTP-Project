#ifndef SNOWFLAKE_TEXTURE2D_HPP
#define SNOWFLAKE_TEXTURE2D_HPP

#include "pch.hpp"
#include "renderer/SamplerFilter.hpp"
#include "renderer/vk/Vulkan.hpp"

// Forward Declarations
struct TextureData;

struct Texture2D
{
  // --- CREATE --- //
  b8 CreateImageFromRawData(const TextureData* textureData);
  b8 CreateImageArrayFromRawData(TextureData* const* textureData, u32 textureCount);

  // --- SAMPLER --- //
  b8 CreateSampler(SamplerFilter samplerFilter        = SamplerFilter::DEFAULT,
                   VkSamplerAddressMode uvAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

  // --- DESTROY --- //
  void DestroyImage();
  void DestroySampler();

  vk::Image image       = {};
  VkImageView imageView = VK_NULL_HANDLE;

  VkSampler sampler = VK_NULL_HANDLE;
};

#endif //SNOWFLAKE_TEXTURE2D_HPP
