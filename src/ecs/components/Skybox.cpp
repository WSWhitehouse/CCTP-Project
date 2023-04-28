#include "ecs/components/Skybox.hpp"

// core
#include "core/Logging.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/ComponentFactory.hpp"

using namespace ECS;

static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

// Forward Declarations
static void CreatePipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
static void CleanUpPipeline();
static INLINE void CreateBuffersAndDescriptorSets(Skybox* skybox);

struct UBOSkyboxData
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::vec3 colour;
};

void ComponentFactory::SkyboxCreate(Skybox* skybox, const SkyboxCreateInfo& createInfo)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  const vk::Device& device = Renderer::GetDevice();

  constexpr const u32 ARRAY_LAYERS = 6;
  FArray<TextureData*, ARRAY_LAYERS> textureData = {nullptr};

  u32 width, height;

  // Load textures...
  {
    for (u32 i = 0; i < ARRAY_LAYERS; i++)
    {
      textureData[i] = AssetDatabase::LoadTexture(createInfo.texturePaths[i]);
      if (textureData[i] == nullptr) ABORT(ABORT_CODE_ASSET_FAILURE);
    }

    width  = textureData[0]->width;
    height = textureData[0]->height;
    for (u32 i = 1; i < ARRAY_LAYERS; ++i)
    {
      if (textureData[i]->width  != width ||
          textureData[i]->height != height)
      {
        LOG_FATAL("Skybox Images are not the same Size! All textures must be the same width/height!");
        ABORT(ABORT_CODE_ASSET_FAILURE);
      }
    }
  }

  // Create VkImage...
  {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = ARRAY_LAYERS;
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
    imageCreateInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    skybox->textureImage.Create(device, &imageCreateInfo);
  }

  // Copy image data into image...
  {
    vk::Image::TransitionLayout(skybox->textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ARRAY_LAYERS);

    const VkDeviceSize imageSize = (VkDeviceSize)((u64)width * (u64)height * 4);
    FArray<vk::Buffer, ARRAY_LAYERS> stagingBuffers = {};

    vk::CommandPool cmdPool   = Renderer::GetGraphicsCommandPool();
    VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

    for (u32 i = 0; i < ARRAY_LAYERS; i++)
    {
      // Create and bind staging buffer...
      stagingBuffers[i].Create(device, imageSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      void *data;
      stagingBuffers[i].MapMemory(device, &data);
      mem_copy(data, textureData[i]->pixels, imageSize);
      stagingBuffers[i].UnmapMemory(device);

      // NOTE(WSWhitehouse): No longer need the pixel data from the image, so free it
      AssetDatabase::FreeTexture(textureData[i]);

      // Set up copy command...
      VkBufferImageCopy copy = {};
      copy.bufferOffset      = 0;
      copy.bufferRowLength   = 0;
      copy.bufferImageHeight = 0;

      copy.imageSubresource =
        {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel       = 0,
          .baseArrayLayer = i,
          .layerCount     = 1
        };

      VkOffset3D offset = {0, 0, 0};
      copy.imageOffset = offset;

      copy.imageExtent =
        {
          .width  = width,
          .height = height,
          .depth  = 1
        };

      vkCmdCopyBufferToImage(cmdBuffer, stagingBuffers[i].buffer, skybox->textureImage.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
    vk::Image::TransitionLayout(skybox->textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ARRAY_LAYERS);

    for (u32 i = 0; i < ARRAY_LAYERS; i++)
    {
      stagingBuffers[i].Destroy(device);
    }
  }

  // Create image view...
  {
    VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.image            = skybox->textureImage.image;
    imageViewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewCreateInfo.format           = VK_FORMAT_R8G8B8A8_SRGB;
    imageViewCreateInfo.subresourceRange =
      {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = ARRAY_LAYERS
      };

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &imageViewCreateInfo, nullptr, &skybox->textureImageView));
  }

  // Create image sampler...
  {
    VkSamplerCreateInfo samplerCreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerCreateInfo.anisotropyEnable        = VK_TRUE;
    samplerCreateInfo.maxAnisotropy           = device.properties.limits.maxSamplerAnisotropy;
    samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.mipLodBias              = 0.0f;
    samplerCreateInfo.minLod                  = 0.0f;
    samplerCreateInfo.maxLod                  = 0.0f;

    VK_SUCCESS_CHECK(vkCreateSampler(device.logicalDevice, &samplerCreateInfo, nullptr, &skybox->textureSampler));
  }

  CreateBuffersAndDescriptorSets(skybox);
}

void ComponentFactory::SkyboxDestroy(Skybox* skybox)
{
  const vk::Device& device = Renderer::GetDevice();
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), MAX_FRAMES_IN_FLIGHT, skybox->descriptorSets.data);
  vkDestroySampler(device.logicalDevice, skybox->textureSampler, nullptr);
  vkDestroyImageView(device.logicalDevice, skybox->textureImageView, nullptr);
  skybox->textureImage.Destroy(device);
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create descriptor sets...
  {
    VkDescriptorSetLayoutBinding skyboxDataLayoutBinding = {};
    skyboxDataLayoutBinding.binding            = 0;
    skyboxDataLayoutBinding.descriptorCount    = 1;
    skyboxDataLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    skyboxDataLayoutBinding.pImmutableSamplers = nullptr;
    skyboxDataLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding skyboxSamplerLayoutBinding = {};
    skyboxSamplerLayoutBinding.binding            = 1;
    skyboxSamplerLayoutBinding.descriptorCount    = 1;
    skyboxSamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skyboxSamplerLayoutBinding.pImmutableSamplers = nullptr;
    skyboxSamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    const VkDescriptorSetLayoutBinding bindings[] =
      {
        skyboxDataLayoutBinding,
        skyboxSamplerLayoutBinding,
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  // Load shader code from file
  FileSystem::FileContent vertShaderCode, fragShaderCode;

  if (!FileSystem::ReadAllFileContent("shaders/skybox.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
  if (!FileSystem::ReadAllFileContent("shaders/skybox.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

  // Create shader modules
  VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode, nullptr);
  VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode, nullptr);

  // Shader stage creation
  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

  // Vertex Input
  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputInfo.vertexBindingDescriptionCount   = 0;
  vertexInputInfo.pVertexBindingDescriptions      = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth               = 1.0f;
  rasterizer.cullMode                = VK_CULL_MODE_FRONT_BIT;
  rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable         = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp          = 0.0f;
  rasterizer.depthBiasSlopeFactor    = 0.0f;

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling.sampleShadingEnable   = VK_FALSE;
  multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading      = 1.0f;
  multisampling.pSampleMask           = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable      = VK_FALSE;

  // Colour Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable         = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = 1;
  colorBlending.pAttachments      = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 1.0f;
  colorBlending.blendConstants[1] = 1.0f;
  colorBlending.blendConstants[2] = 1.0f;
  colorBlending.blendConstants[3] = 1.0f;

  // Depth Stencil
  VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depthStencil.depthTestEnable       = VK_TRUE;
  depthStencil.depthWriteEnable      = VK_TRUE;
  depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds        = 0.0f;
  depthStencil.maxDepthBounds        = 1.0f;
  depthStencil.stencilTestEnable     = VK_FALSE;
  depthStencil.front                 = {};
  depthStencil.back                  = {};

  // Pipeline Layout
  const VkDescriptorSetLayout descriptorSetLayouts[] =
    {
      descriptorSetLayout
    };

  const GraphicsPipelineConfig config
    {
      .renderFuncPtr  = Render,
      .cleanUpFuncPtr = CleanUpPipeline,

      .renderQueue = SKYBOX_QUEUE,

      .renderPass    = 0,
      .renderSubpass = 0,

      .shaderStageCreateInfoCount = ARRAY_SIZE(shaderStages),
      .shaderStageCreateInfos     = shaderStages,

      .vertexInputStateCreateInfo   = &vertexInputInfo,
      .rasterizationStateCreateInfo = &rasterizer,
      .multisampleStateCreateInfo   = &multisampling,
      .colourBlendStateCreateInfo   = &colorBlending,
      .depthStencilStateCreateInfo  = &depthStencil,

      .descriptorSetLayoutCount = ARRAY_SIZE(descriptorSetLayouts),
      .descriptorSetLayouts     = descriptorSetLayouts,

      .pushConstantRangeCount = 0,
      .pushConstantRanges     = nullptr,
    };

  pipelineHandle = Renderer::CreateGraphicsPipeline(config);

  // Destroy shader modules
  vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

  // Clean up file content
  mem_free(vertShaderCode.data);
  mem_free(fragShaderCode.data);
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* skyboxSparseSet = ecs.GetComponentSparseSet<Skybox>();
  for (u32 skyboxIndex = 0; skyboxIndex < skyboxSparseSet->componentCount; ++skyboxIndex)
  {
    ComponentData<Skybox>& componentData = ((ComponentData<Skybox>*)skyboxSparseSet->componentArray)[skyboxIndex];
    Skybox& skybox = componentData.component;

    const Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    UBOSkyboxData* skyboxModelData = (UBOSkyboxData*)skybox.skyboxDataUBOMapped[currentFrame];
    mem_zero(skyboxModelData, sizeof(UBOSkyboxData));
    skyboxModelData->WVP    = transform->GetWVPMatrix(camera);
    skyboxModelData->colour = skybox.colour;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &skybox.descriptorSets[currentFrame], 0, nullptr);

    // Render cube...
    vkCmdDraw(cmdBuffer, 36, 1, 0, 0);
  }
}

static void CleanUpPipeline()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static INLINE void CreateBuffersAndDescriptorSets(Skybox* skybox)
{
  const vk::Device& device = Renderer::GetDevice();

  // Skybox Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& modelDataUB     = skybox->skyboxDataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& modelDataMapped = skybox->skyboxDataUBOMapped;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = modelDataUB[i].Create(device, sizeof(UBOSkyboxData),
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Skybox Data Buffer %i Creation Failed!", i);
        return;
      }

      modelDataUB[i].MapMemory(device, &modelDataMapped[i]);
    }
  }

  FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = skybox->descriptorSets;

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
    // Skybox Data UBO
    VkDescriptorBufferInfo skyboxDataBufferInfo = {};
    skyboxDataBufferInfo.buffer = skybox->skyboxDataUBO[i].buffer;
    skyboxDataBufferInfo.offset = 0;
    skyboxDataBufferInfo.range  = sizeof(UBOSkyboxData);

    VkWriteDescriptorSet skyboxDataDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    skyboxDataDescriptorWrite.dstSet           = descriptorSets[i];
    skyboxDataDescriptorWrite.dstBinding       = 0;
    skyboxDataDescriptorWrite.dstArrayElement  = 0;
    skyboxDataDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    skyboxDataDescriptorWrite.descriptorCount  = 1;
    skyboxDataDescriptorWrite.pBufferInfo      = &skyboxDataBufferInfo;
    skyboxDataDescriptorWrite.pImageInfo       = nullptr;
    skyboxDataDescriptorWrite.pTexelBufferView = nullptr;

    // Skybox image
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = skybox->textureImageView;
    imageInfo.sampler     = skybox->textureSampler;

    VkWriteDescriptorSet imageDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    imageDescriptorWrite.dstSet          = descriptorSets[i];
    imageDescriptorWrite.dstBinding      = 1;
    imageDescriptorWrite.dstArrayElement = 0;
    imageDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageDescriptorWrite.descriptorCount = 1;
    imageDescriptorWrite.pImageInfo      = &imageInfo;

    VkWriteDescriptorSet descriptorWrites[] =
      {
        skyboxDataDescriptorWrite,
        imageDescriptorWrite
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }
}