#include "PointCloudRenderer.hpp"

// core
#include "core/Logging.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// geometry
#include "geometry/PointCloud.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/ComponentFactory.hpp"
#include "ecs/components/Transform.hpp"

using namespace ECS;

// Forward Declarations
static void CreatePipeline();
static void CleanUpPipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
static vk::Buffer CreatePointBuffer(const glm::vec3* pointArray, u64 pointCount);

static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

struct UBOModelData
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::mat4 worldMat;
  alignas(16) glm::mat4 invWorldMat;
};

STATIC_ASSERT(sizeof(UBOModelData) % 16 == 0);

void ComponentFactory::PointCloudRendererCreate(PointCloudRenderer* pointCloudRenderer, const PointCloud* pointCloud)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  const vk::Device& device = Renderer::GetDevice();

  pointCloudRenderer->pointBuffer = CreatePointBuffer(pointCloud->points, pointCloud->pointCount);
  pointCloudRenderer->pointCount  = pointCloud->pointCount;

  // Model Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& modelDataUB     = pointCloudRenderer->modelDataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& modelDataMapped = pointCloudRenderer->modelDataUBOMapped;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = modelDataUB[i].Create(device, sizeof(UBOModelData),
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Model Data Buffer %i Creation Failed!", i);
        return;
      }

      modelDataUB[i].MapMemory(device, &modelDataMapped[i]);
    }
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = pointCloudRenderer->descriptorSets;

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
      // Model Data UBO
      VkDescriptorBufferInfo modelDataBufferInfo = {};
      modelDataBufferInfo.buffer = pointCloudRenderer->modelDataUBO[i].buffer;
      modelDataBufferInfo.offset = 0;
      modelDataBufferInfo.range  = sizeof(UBOModelData);

      VkWriteDescriptorSet modelDataDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      modelDataDescriptorWrite.dstSet           = descriptorSets[i];
      modelDataDescriptorWrite.dstBinding       = 0;
      modelDataDescriptorWrite.dstArrayElement  = 0;
      modelDataDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      modelDataDescriptorWrite.descriptorCount  = 1;
      modelDataDescriptorWrite.pBufferInfo      = &modelDataBufferInfo;
      modelDataDescriptorWrite.pImageInfo       = nullptr;
      modelDataDescriptorWrite.pTexelBufferView = nullptr;

      const VkWriteDescriptorSet descriptorWrites[] =
        {
          modelDataDescriptorWrite,
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }
}

void ComponentFactory::PointCloudRendererDestroy(PointCloudRenderer* pointCloudRenderer)
{
  const vk::Device& device = Renderer::GetDevice();

  pointCloudRenderer->pointBuffer.Destroy(device);

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    pointCloudRenderer->modelDataUBO[i].UnmapMemory(device);
    pointCloudRenderer->modelDataUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(),
                       MAX_FRAMES_IN_FLIGHT, pointCloudRenderer->descriptorSets.data);
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create Mesh Descriptor Set Layout
  {
    LOG_INFO("\tCreating Point Cloud Descriptor Set Layout...");

    VkDescriptorSetLayoutBinding modelDataLayoutBinding = {};
    modelDataLayoutBinding.binding            = 0;
    modelDataLayoutBinding.descriptorCount    = 1;
    modelDataLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    modelDataLayoutBinding.pImmutableSamplers = nullptr;
    modelDataLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        modelDataLayoutBinding,
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));

    LOG_INFO("\tPoint Cloud Descriptor Set Layout Created!");
  }

  // Create Graphics Pipeline
  {
    // Load shader code from file
    FileSystem::FileContent vertShaderCode, fragShaderCode;

    if (!FileSystem::ReadAllFileContent("shaders/pointcloud/pointCloud.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
    if (!FileSystem::ReadAllFileContent("shaders/pointcloud/pointCloud.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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

    const VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Vertex Input
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(glm::vec3);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescription = {};
    attributeDescription.binding  = 0;
    attributeDescription.location = 0;
    attributeDescription.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions    = &attributeDescription;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_FILL VK_POLYGON_MODE_LINE
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT VK_CULL_MODE_NONE
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

    const GraphicsPipelineConfig config
      {
        .renderFuncPtr  = Render,
        .cleanUpFuncPtr = CleanUpPipeline,

        .renderQueue = OPAQUE_QUEUE,

        .renderPass    = 0,
        .renderSubpass = 0,

        .shaderStageCreateInfoCount = ARRAY_SIZE(shaderStages),
        .shaderStageCreateInfos     = shaderStages,

        .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,

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

    LOG_INFO("\tPoint Cloud Renderer Graphics Pipeline Created!");
  }
}

static void CleanUpPipeline()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<PointCloudRenderer>();
  for (u32 i = 0; i < sparseSet->componentCount; ++i)
  {
    ComponentData<PointCloudRenderer>& componentData = ((ComponentData<PointCloudRenderer>*)sparseSet->componentArray)[i];
    PointCloudRenderer& pointCloudRenderer = componentData.component;

    if (!pointCloudRenderer.render) continue;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    UBOModelData modelData = {};
    modelData.WVP      = transform->GetWVPMatrix(camera);
    modelData.worldMat = transform->matrix;
    mem_copy(pointCloudRenderer.modelDataUBOMapped[currentFrame], &modelData, sizeof(modelData));

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &pointCloudRenderer.descriptorSets[currentFrame], 0, nullptr);

    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &pointCloudRenderer.pointBuffer.buffer, offsets);
    vkCmdDraw(cmdBuffer, pointCloudRenderer.pointCount, 1, 0, 0);
  }
}

static vk::Buffer CreatePointBuffer(const glm::vec3* pointArray, u64 pointCount)
{
  const vk::Device& device = Renderer::GetDevice();

  vk::Buffer pointsBuffer = {};

  VkDeviceSize bufferSize = sizeof(pointArray[0]) * pointCount;

  // Create staging buffer
  vk::Buffer stagingBuffer;
  stagingBuffer.Create(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Copy data to staging buffer
  void* data;
  stagingBuffer.MapMemory(device, &data);
  mem_copy(data, pointArray, bufferSize);
  stagingBuffer.UnmapMemory(device);

  // Create points buffer
  pointsBuffer.Create(device, bufferSize,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy staging buffer to vertex buffer
  vk::Buffer::CopyBufferToBuffer(stagingBuffer, pointsBuffer, bufferSize);

  // Destroy staging buffer
  stagingBuffer.Destroy(device);

  return pointsBuffer;
}