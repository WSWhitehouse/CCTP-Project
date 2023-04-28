#include "renderer/Material.hpp"
// core
#include "core/Logging.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"

Material* Material::CreateMaterial(const char* albedoTexPath, glm::vec3 colour)
{
  Material* material = (Material*)mem_alloc(sizeof(Material));

  if (albedoTexPath == nullptr)
  {
    // TODO(WSWhitehouse): Set default texture
    return material;
  }

  TextureData* texData = AssetDatabase::LoadTexture(albedoTexPath);
  if (texData == nullptr)
  {
    mem_free(material);
    return nullptr;
  }

  const vk::Device& device = Renderer::GetDevice();

  b8 imageCreationSuccess = material->albedo.CreateImageFromRawData(texData);
  AssetDatabase::FreeTexture(texData);

  if (!imageCreationSuccess)
  {
    mem_free(material);
    return nullptr;
  }

  b8 samplerCreationSuccess = material->albedo.CreateSampler(SamplerFilter::POINT);
  if (!samplerCreationSuccess)
  {
    mem_free(material);
    return nullptr;
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = material->descriptorSets;

    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {};
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      layouts[i] = descriptorSetLayout;
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool     = Renderer::GetDescriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    descriptorSetAllocateInfo.pSetLayouts        = layouts;

    VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice,
                                              &descriptorSetAllocateInfo,
                                              descriptorSets.data));

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      VkDescriptorImageInfo imageInfo = {};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView   = material->albedo.imageView;
      imageInfo.sampler     = material->albedo.sampler;

      VkWriteDescriptorSet imageDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      imageDescriptorWrite.dstSet          = descriptorSets[i];
      imageDescriptorWrite.dstBinding      = 0;
      imageDescriptorWrite.dstArrayElement = 0;
      imageDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      imageDescriptorWrite.descriptorCount = 1;
      imageDescriptorWrite.pImageInfo      = &imageInfo;

      VkWriteDescriptorSet descriptorWrites[] =
        {
          imageDescriptorWrite
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }

  return material;
}

void Material::DestroyMaterial(Material* material)
{
  const vk::Device& device = Renderer::GetDevice();
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), MAX_FRAMES_IN_FLIGHT, material->descriptorSets.data);
  material->albedo.DestroySampler();
  material->albedo.DestroyImage();
  mem_free(material);
}

void Material::InitMaterialSystem()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create Descriptor Set Layout
  {
    VkDescriptorSetLayoutBinding textureSamplerLayoutBinding = {};
    textureSamplerLayoutBinding.binding            = 0;
    textureSamplerLayoutBinding.descriptorCount    = 1;
    textureSamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureSamplerLayoutBinding.pImmutableSamplers = nullptr;
    textureSamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        textureSamplerLayoutBinding,
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  defaultMaterial = CreateMaterial("data/texture.jpg");
}

void Material::ShutdownMaterialSystem()
{
  DestroyMaterial(defaultMaterial);

  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}