#include "ecs/components/Sprite.hpp"

// core
#include "core/Logging.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// threading
#include "threading/JobSystem.hpp"

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
static void CleanUp();
static INLINE void CreateBuffersAndDescriptorSets(Sprite* sprite);

struct UBOSpriteData
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::mat4 worldMat;
  alignas(8)  glm::vec2 textureSize;
  alignas(8)  glm::vec2 size;
};

struct SpritePushConstant
{
  alignas(16) glm::vec3 colour;
  alignas(8)  glm::vec2 uvMultiplier;
  alignas(4)  f32 textureIndex;
};

void ComponentFactory::SpriteCreate(Sprite* sprite, const SpriteCreateInfo& createInfo)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  ASSERT_MSG(createInfo.textureCount > 0, "Texture count must be greater than 0!");

  sprite->billboard = createInfo.billboardType;

  if (createInfo.textureCount == 1)
  {
    TextureData* texData = AssetDatabase::LoadTexture(createInfo.texturePath[0]);
    if (texData == nullptr) return;

    sprite->textureSize     = glm::vec2(texData->width, texData->height);
    sprite->textureCount    = 1;
    sprite->currentTexIndex = 0;

    b8 imageCreationSuccess = sprite->texture.CreateImageFromRawData(texData);
    AssetDatabase::FreeTexture(texData);
    if (!imageCreationSuccess)
    {
      LOG_FATAL("Failed to create sprite image!");
      return;
    }
  }
  else
  {
    TextureData** texData = (TextureData**)mem_alloc(sizeof(TextureData*) * createInfo.textureCount);

    for (u32 i = 0; i < createInfo.textureCount; ++i)
    {
      texData[i] = AssetDatabase::LoadTexture(createInfo.texturePath[i]);
      if (texData[i] == nullptr) return;
    }

    const u32 width  = texData[0]->width;
    const u32 height = texData[0]->height;
    sprite->textureSize = glm::vec2(width, height);

    sprite->textureCount    = createInfo.textureCount;
    sprite->currentTexIndex = 0;

    b8 imageCreationSuccess = sprite->texture.CreateImageArrayFromRawData(texData, createInfo.textureCount);

    for (u32 i = 0; i < createInfo.textureCount; ++i)
    {
      AssetDatabase::FreeTexture(texData[i]);
    }
    mem_free(texData);

    if (!imageCreationSuccess)
    {
      LOG_FATAL("Failed to create sprite image!");
      return;
    }
  }

  b8 samplerCreationSuccess = sprite->texture.CreateSampler(createInfo.samplerFilter);
  if (!samplerCreationSuccess)
  {
    LOG_FATAL("Failed to create image sampler!");
    return;
  }

  CreateBuffersAndDescriptorSets(sprite);
}

static void Destroy(Sprite sprite, u32 startFrame)
{
  Renderer::WaitForFrame(startFrame + MAX_FRAMES_IN_FLIGHT);

  const vk::Device& device = Renderer::GetDevice();

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    sprite.spriteDataUBO[i].UnmapMemory(device);
    sprite.spriteDataUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), MAX_FRAMES_IN_FLIGHT, sprite.descriptorSets.data);

  sprite.texture.DestroySampler();
  sprite.texture.DestroyImage();
}

void ComponentFactory::SpriteDestroy(Sprite* sprite, b8 immediate)
{
  if (!immediate)
  {
    Sprite copy    = *sprite;
    u32 startFrame = Renderer::GetFrameNumber();
    JobSystem::WorkFuncPtr lambda = [=] { Destroy(copy, startFrame); };

    JobSystem::SubmitJob(lambda);
    return;
  }

  const vk::Device& device = Renderer::GetDevice();

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    sprite->spriteDataUBO[i].UnmapMemory(device);
    sprite->spriteDataUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), MAX_FRAMES_IN_FLIGHT, sprite->descriptorSets.data);

  sprite->texture.DestroySampler();
  sprite->texture.DestroyImage();
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create Descriptor Set Layout
  {
    VkDescriptorSetLayoutBinding spriteDataLayoutBinding = {};
    spriteDataLayoutBinding.binding            = 0;
    spriteDataLayoutBinding.descriptorCount    = 1;
    spriteDataLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    spriteDataLayoutBinding.pImmutableSamplers = nullptr;
    spriteDataLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding textureSamplerLayoutBinding = {};
    textureSamplerLayoutBinding.binding            = 1;
    textureSamplerLayoutBinding.descriptorCount    = 1;
    textureSamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureSamplerLayoutBinding.pImmutableSamplers = nullptr;
    textureSamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;


    const VkDescriptorSetLayoutBinding bindings[] =
      {
        spriteDataLayoutBinding,
        textureSamplerLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  // Create Graphics Pipeline
  {
    // Load shader code from file
    FileSystem::FileContent vertShaderCode, fragShaderCode;

    if (!FileSystem::ReadAllFileContent("shaders/sprite.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
    if (!FileSystem::ReadAllFileContent("shaders/sprite.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT
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
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
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

    const VkPushConstantRange pushConstantRange
      {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(SpritePushConstant)
      };

    const GraphicsPipelineConfig config
      {
        .renderFuncPtr  = Render,
        .cleanUpFuncPtr = CleanUp,

        .renderQueue = TRANSPARENT_QUEUE,

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

        .pushConstantRangeCount = 1,
        .pushConstantRanges     = &pushConstantRange,
      };

    pipelineHandle = Renderer::CreateGraphicsPipeline(config);

    // Destroy shader modules
    vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

    // Clean up file content
    mem_free(vertShaderCode.data);
    mem_free(fragShaderCode.data);
  }
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline    = Renderer::GetGraphicsPipeline(pipelineHandle);
  ComponentSparseSet* spriteSparseSet = ecs.GetComponentSparseSet<Sprite>();

  if (spriteSparseSet->componentCount <= 0) return;

  for (u32 spriteIndex = 0; spriteIndex < spriteSparseSet->componentCount; ++spriteIndex)
  {
    ComponentData<Sprite>& componentData = ((ComponentData<Sprite>*)spriteSparseSet->componentArray)[spriteIndex];
    Sprite& sprite = componentData.component;

    if (!sprite.render) continue;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    glm::mat4 wvMat = camera.viewMatrix * transform->matrix;
    // https://www.geeks3d.com/20140807/billboarding-vertex-shader-glsl/
    if (sprite.billboard != Sprite::BillboardType::DISABLED)
    {
      wvMat[0][0] = 1.0f;
      wvMat[0][1] = 0.0f;
      wvMat[0][2] = 0.0f;

      if (sprite.billboard == Sprite::BillboardType::SPHERICAL)
      {
        wvMat[1][0] = 0.0f;
        wvMat[1][1] = 1.0f;
        wvMat[1][2] = 0.0f;
      }

      wvMat[2][0] = 0.0f;
      wvMat[2][1] = 0.0f;
      wvMat[2][2] = 1.0f;
    }

    UBOSpriteData* spriteData = (UBOSpriteData*)sprite.spriteDataUBOMapped[currentFrame];
    mem_zero(spriteData, sizeof(UBOSpriteData));
    spriteData->WVP         = camera.projMatrix * wvMat;
    spriteData->worldMat    = transform->matrix;
    spriteData->textureSize = sprite.textureSize;
    spriteData->size        = sprite.size;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &sprite.descriptorSets[currentFrame], 0, nullptr);

    const SpritePushConstant pushConstant
      {
        .colour       = sprite.colour,
        .uvMultiplier = sprite.uvMultiplier,
        .textureIndex = (f32)sprite.currentTexIndex
      };

    vkCmdPushConstants(cmdBuffer, pipeline.layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstant), &pushConstant);

    // Render quad...
    vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
  }
}

static void CleanUp()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static INLINE void CreateBuffersAndDescriptorSets(Sprite* sprite)
{
  const vk::Device& device = Renderer::GetDevice();

  // Create model data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& spriteDataUBO       = sprite->spriteDataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& spriteDataUBOMapped = sprite->spriteDataUBOMapped;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = spriteDataUBO[i].Create(device, sizeof(UBOSpriteData),
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Sprite Data Buffer %i Creation Failed!", i);
        return;
      }

      spriteDataUBO[i].MapMemory(device, &spriteDataUBOMapped[i]);
    }
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = sprite->descriptorSets;

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
      // Sprite Data UBO
      VkDescriptorBufferInfo modelDataBufferInfo = {};
      modelDataBufferInfo.buffer = sprite->spriteDataUBO[i].buffer;
      modelDataBufferInfo.offset = 0;
      modelDataBufferInfo.range  = sizeof(UBOSpriteData);

      VkWriteDescriptorSet modelDataDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      modelDataDescriptorWrite.dstSet           = descriptorSets[i];
      modelDataDescriptorWrite.dstBinding       = 0;
      modelDataDescriptorWrite.dstArrayElement  = 0;
      modelDataDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      modelDataDescriptorWrite.descriptorCount  = 1;
      modelDataDescriptorWrite.pBufferInfo      = &modelDataBufferInfo;
      modelDataDescriptorWrite.pImageInfo       = nullptr;
      modelDataDescriptorWrite.pTexelBufferView = nullptr;

      // Image info
      VkDescriptorImageInfo imageInfo = {};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView   = sprite->texture.imageView;
      imageInfo.sampler     = sprite->texture.sampler;

      VkWriteDescriptorSet imageDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      imageDescriptorWrite.dstSet          = descriptorSets[i];
      imageDescriptorWrite.dstBinding      = 1;
      imageDescriptorWrite.dstArrayElement = 0;
      imageDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      imageDescriptorWrite.descriptorCount = 1;
      imageDescriptorWrite.pImageInfo      = &imageInfo;

      const VkWriteDescriptorSet descriptorWrites[] =
        {
          modelDataDescriptorWrite,
          imageDescriptorWrite
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }
}