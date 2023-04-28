#include "ecs/components/MeshRenderer.hpp"

// core
#include "core/Logging.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"
#include "renderer/Material.hpp"

// geometry
#include "geometry/MeshGeometry.hpp"
#include "geometry/Mesh.hpp"
#include "geometry/Vertex.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/ComponentFactory.hpp"

using namespace ECS;

// Forward Declarations
static void CreatePipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
static void CleanUp();
static INLINE void CreateBuffersAndDescriptorSets(MeshRenderer* meshRenderer);
vk::Buffer CreateVertexBuffer(const Vertex* vertexArray, u64 vertexCount);
vk::Buffer CreateIndexBuffer(const void* indexArray, u64 indexCount, u64 sizeOfIndex);

static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

struct UBOModelData
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::mat4 worldMat;
  alignas(16) glm::mat4 invWorldMat;
};

STATIC_ASSERT(sizeof(UBOModelData) % 16 == 0);

void ComponentFactory::MeshRendererCreate(MeshRenderer* meshRenderer, const Mesh* mesh)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  // Set default values
  meshRenderer->colour    = glm::vec3(1.0f, 1.0f, 1.0f);
  meshRenderer->texTiling = glm::vec2(1.0f, 1.0f);

  // Allocate renderer data array
  meshRenderer->bufferDataCount = mesh->nodeCount;
  meshRenderer->bufferDataArray = (MeshBufferData*)(mem_alloc(sizeof(MeshBufferData) * mesh->nodeCount));

  for (u32 i = 0; i < mesh->nodeCount; i++)
  {
    const MeshNode& node         = mesh->nodeArray[i];
    const MeshGeometry& geometry = mesh->geometryArray[node.geometryIndex];

    // REVIEW(WSWhitehouse): Is there a better way to create this temp array that isn't each loop iteration?
    const u64 vertexArraySize = sizeof(Vertex) * geometry.vertexCount;
    Vertex* vertexArray = (Vertex*)(mem_alloc(vertexArraySize));
    mem_copy(vertexArray, geometry.vertexArray, vertexArraySize);

    // Apply node transformation matrix to vertices...
    for (u64 vertIndex = 0; vertIndex < geometry.vertexCount; vertIndex++)
    {
      Vertex& vertex  = vertexArray[vertIndex];
      vertex.position = node.transformMatrix * glm::vec4(vertex.position, 1.0f);
    }

    // Create buffers and initialise renderer data...
    {
      MeshBufferData& rendererData = meshRenderer->bufferDataArray[i];

      rendererData.vertexBuffer = CreateVertexBuffer(vertexArray, geometry.vertexCount);

      switch (geometry.indexType)
      {
        case IndexType::U16_TYPE:
        {
          rendererData.indexType = VK_INDEX_TYPE_UINT16;
          break;
        }
        case IndexType::U32_TYPE:
        {
          rendererData.indexType = VK_INDEX_TYPE_UINT32;
          break;
        }

        default:
        {
          LOG_FATAL("Unhandled Mesh Geometry Index Type Case!");
          break;
        }
      }

      rendererData.indexBuffer = CreateIndexBuffer(geometry.indexArray, geometry.indexCount, geometry.SizeOfIndex());
      rendererData.indexCount  = geometry.indexCount;
    }

    mem_free(vertexArray);
  }

  CreateBuffersAndDescriptorSets(meshRenderer);
}

void ComponentFactory::MeshRendererCreate(MeshRenderer* meshRenderer, const MeshGeometry& mesh)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  // Set default values
  meshRenderer->colour    = glm::vec3(1.0f, 1.0f, 1.0f);
  meshRenderer->texTiling = glm::vec2(1.0f, 1.0f);

  // Allocate renderer data array
  meshRenderer->bufferDataCount = 1;
  meshRenderer->bufferDataArray = (MeshBufferData*)(mem_alloc(sizeof(MeshBufferData)));

  // Create buffers and initialise renderer data...
  {
    MeshBufferData& rendererData = meshRenderer->bufferDataArray[0];

    rendererData.vertexBuffer = CreateVertexBuffer(mesh.vertexArray, mesh.vertexCount);

    switch (mesh.indexType)
    {
      case IndexType::U16_TYPE:
      {
        rendererData.indexType = VK_INDEX_TYPE_UINT16;
        break;
      }
      case IndexType::U32_TYPE:
      {
        rendererData.indexType = VK_INDEX_TYPE_UINT32;
        break;
      }

      default:
      {
        LOG_FATAL("Unhandled Mesh Geometry Index Type Case!");
        break;
      }
    }

    rendererData.indexBuffer = CreateIndexBuffer(mesh.indexArray, mesh.indexCount, mesh.SizeOfIndex());
    rendererData.indexCount  = mesh.indexCount;
  }

  CreateBuffersAndDescriptorSets(meshRenderer);
}

void ComponentFactory::MeshRendererDestroy(MeshRenderer* meshRenderer, b8 destroyMaterials)
{
  const vk::Device& device = Renderer::GetDevice();

  for (u32 i = 0; i < meshRenderer->bufferDataCount; i++)
  {
    MeshBufferData& rendererData = meshRenderer->bufferDataArray[i];
    rendererData.vertexBuffer.Destroy(device);
    rendererData.indexBuffer.Destroy(device);
  }

  if (destroyMaterials && meshRenderer->material != nullptr)
  {
    Material::DestroyMaterial(meshRenderer->material);
  }

  mem_free(meshRenderer->bufferDataArray);

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    meshRenderer->modelDataUBO[i].UnmapMemory(device);
    meshRenderer->modelDataUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(),
                       MAX_FRAMES_IN_FLIGHT, meshRenderer->descriptorSets.data);
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* meshRendererSparseSet = ecs.GetComponentSparseSet<MeshRenderer>();
  for (u32 meshIndex = 0; meshIndex < meshRendererSparseSet->componentCount; ++meshIndex)
  {
    ComponentData<MeshRenderer>& componentData = ((ComponentData<MeshRenderer>*)meshRendererSparseSet->componentArray)[meshIndex];
    MeshRenderer& meshRenderer = componentData.component;

    if (!meshRenderer.renderMesh) continue;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    UBOModelData modelData = {};
    modelData.WVP      = transform->GetWVPMatrix(camera);
    modelData.worldMat = transform->matrix;
    mem_copy(meshRenderer.modelDataUBOMapped[currentFrame], &modelData, sizeof(modelData));

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &meshRenderer.descriptorSets[currentFrame], 0, nullptr);

    Material* material = meshRenderer.material;
    if (material == nullptr) material = Material::defaultMaterial;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            2, 1, &material->descriptorSets[currentFrame], 0, nullptr);

    const Material::PushConstants pushConstant
      {
        .colour    = meshRenderer.colour,
        .texTiling = meshRenderer.texTiling,
      };

    vkCmdPushConstants(cmdBuffer, pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(Material::PushConstants), &pushConstant);

    // Draw mesh buffers...
    for (u32 i = 0; i < meshRenderer.bufferDataCount; i++)
    {
      const MeshBufferData& rendererData = meshRenderer.bufferDataArray[i];

      const VkDeviceSize offsets[] = { 0 };
      vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &rendererData.vertexBuffer.buffer, offsets);
      vkCmdBindIndexBuffer(cmdBuffer, rendererData.indexBuffer.buffer, 0, rendererData.indexType);
      vkCmdDrawIndexed(cmdBuffer, rendererData.indexCount, 1, 0, 0, 0);
    }
  }
}

static void CleanUp()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create Mesh Descriptor Set Layout
  {
    LOG_INFO("\tCreating mesh descriptor set layout...");

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

    LOG_INFO("\tMesh descriptor set layout created!");
  }

  // Create Graphics Pipeline
  {
    // Load shader code from file
    FileSystem::FileContent vertShaderCode, fragShaderCode;

    if (!FileSystem::ReadAllFileContent("shaders/default.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
    if (!FileSystem::ReadAllFileContent("shaders/unlit.frag.spv", &fragShaderCode))   ABORT(ABORT_CODE_ASSET_FAILURE);

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
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &Vertex::BindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = Vertex::AttributeDescriptions.Size();
    vertexInputInfo.pVertexAttributeDescriptions    = Vertex::AttributeDescriptions.data;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_FILL VK_POLYGON_MODE_LINE
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_BACK_BIT VK_CULL_MODE_NONE
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
        descriptorSetLayout,
        Material::descriptorSetLayout,
      };

    const VkPushConstantRange pushConstant =
      {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(Material::PushConstants)
      };

    const GraphicsPipelineConfig config
      {
        .renderFuncPtr  = Render,
        .cleanUpFuncPtr = CleanUp,

        .renderQueue = OPAQUE_QUEUE,

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
        .pushConstantRanges     = &pushConstant,
      };

    pipelineHandle = Renderer::CreateGraphicsPipeline(config);

    // Destroy shader modules
    vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

    // Clean up file content
    mem_free(vertShaderCode.data);
    mem_free(fragShaderCode.data);

    LOG_INFO("\tUnlit Graphics Pipeline Created!");
  }
}

static INLINE void CreateBuffersAndDescriptorSets(MeshRenderer* meshRenderer)
{
  const vk::Device& device = Renderer::GetDevice();

  // Model Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& modelDataUB     = meshRenderer->modelDataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& modelDataMapped = meshRenderer->modelDataUBOMapped;

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
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = meshRenderer->descriptorSets;

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
      modelDataBufferInfo.buffer = meshRenderer->modelDataUBO[i].buffer;
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

      VkWriteDescriptorSet descriptorWrites[] =
        {
          modelDataDescriptorWrite,
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }
}

vk::Buffer CreateVertexBuffer(const Vertex* vertexArray, u64 vertexCount)
{
  const vk::Device& device = Renderer::GetDevice();

  vk::Buffer vertexBuffer = {};

  VkDeviceSize bufferSize = sizeof(vertexArray[0]) * vertexCount;

  // Create staging buffer
  vk::Buffer stagingBuffer;
  stagingBuffer.Create(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Copy vertex data to staging buffer
  void* data;
  stagingBuffer.MapMemory(device, &data);
  mem_copy(data, vertexArray, bufferSize);
  stagingBuffer.UnmapMemory(device);

  // Create vertex buffer
  vertexBuffer.Create(device, bufferSize,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy staging buffer to vertex buffer
  vk::Buffer::CopyBufferToBuffer(stagingBuffer, vertexBuffer, bufferSize);

  // Destroy staging buffer
  stagingBuffer.Destroy(device);

  return vertexBuffer;
}

vk::Buffer CreateIndexBuffer(const void* indexArray, u64 indexCount, u64 sizeOfIndex)
{
  const vk::Device& device = Renderer::GetDevice();

  VkDeviceSize bufferSize = sizeOfIndex * indexCount;

  // Create staging buffer
  vk::Buffer stagingBuffer = {};
  stagingBuffer.Create(device, bufferSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Copy index data to staging buffer
  void* data;
  stagingBuffer.MapMemory(device, &data);
  mem_copy(data, indexArray, bufferSize);
  stagingBuffer.UnmapMemory(device);

  // Create index buffer
  vk::Buffer indexBuffer = {};
  indexBuffer.Create(device, bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy staging buffer to index buffer
  vk::Buffer::CopyBufferToBuffer(stagingBuffer, indexBuffer, bufferSize);

  // Destroy staging buffer
  stagingBuffer.Destroy(device);

  return indexBuffer;
}