#include "ecs/components/SdfRenderer.hpp"

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

#include "containers/BSPTree.hpp"

using namespace ECS;

static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

struct SdfDataUBO
{
  alignas(16) glm::mat4x4 WVP;
  alignas(16) glm::mat4x4 worldMat;
  alignas(16) glm::mat4x4 invWorldMat;

  alignas(16) glm::vec3 boundingBoxExtents;
  alignas(04) u32 indexCount;
};

struct BSPNodeUBO
{
  alignas(16) Plane plane = {};

  alignas(4) u32 isLeaf = 0;

  // NOTE(WSWhitehouse): Should only link to other nodes if this is not a leaf
  alignas(4) u32 nodePositive = U32_MAX;
  alignas(4) u32 nodeNegative = U32_MAX;

  alignas(4) u32 indexStart;
  alignas(4) u32 indexCount;
};

// Forward Declarations
static void CreatePipeline();
static void CleanUpPipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);

void ComponentFactory::SdfRendererCreate(ECS::Manager& ecs, SdfRenderer* sdfRenderer, const Mesh* mesh)
{
  UNUSED(ecs);

  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  BSPTree tree = {};
  tree.BuildTree(mesh);

  BSPNodeUBO* nodes = (BSPNodeUBO*)mem_alloc(sizeof(BSPNodeUBO) * tree.nodes.Size());
  u32 indexCount = 0;

  for (u32 i = 0; i < tree.nodes.Size(); ++i)
  {
    nodes[i] =
      {
        .plane        = tree.nodes[i].plane,
        .isLeaf       = tree.nodes[i].isLeaf ? 1U : 0U,
        .nodePositive = tree.nodes[i].nodePositive,
        .nodeNegative = tree.nodes[i].nodeNegative,
        .indexStart   = indexCount,
        .indexCount   = tree.nodes[i].indexCount
      };

    indexCount += tree.nodes[i].indexCount;
  }

  LOG_DEBUG("Index Count: %u", indexCount);
  u32* indices = (u32*)mem_alloc(sizeof(u32) * indexCount);
  u32 currentIndex = 0;
  for (u32 i = 0; i < tree.nodes.Size(); ++i)
  {
    mem_copy(&indices[currentIndex], tree.nodes[i].indices, sizeof(u32) * tree.nodes[i].indexCount);
    currentIndex += tree.nodes[i].indexCount;
  }

//  const MeshGeometry testGeom = {
//    .vertexArray = mesh->geometryArray[0].vertexArray,
//    .indexArray  = indices,
//    .vertexCount = mesh->geometryArray[0].vertexCount,
//    .indexCount  = indexCount,
//    .indexType   = IndexType::U32_TYPE,
//  };

//  Entity entity = ecs.CreateEntity();
//  Transform* testTransform   = ecs.AddComponent<Transform>(entity);
//  testTransform->position    = {0.0f, 4.0f, 0.0f};
//  MeshRenderer* testRenderer = ecs.AddComponent<MeshRenderer>(entity);
//  ComponentFactory::MeshRendererCreate(testRenderer, testGeom);

  const vk::Device& device = Renderer::GetDevice();

  // Sdf Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO       = sdfRenderer->dataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& dataUBOMapped = sdfRenderer->dataUBOMapped;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = dataUBO[i].Create(device, sizeof(SdfDataUBO),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Model Data Buffer %i Creation Failed!", i);
        return;
      }

      dataUBO[i].MapMemory(device, &dataUBOMapped[i]);
    }
  }

  // BSP Nodes
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO = sdfRenderer->bspNodesUBO;
    const VkDeviceSize bufferSize = sizeof(BSPNodeUBO) * tree.nodes.Size();

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = dataUBO[i].Create(device, bufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO BSP Nodes Buffer %i Creation Failed!", i);
        return;
      }

      void* mappedData;
      dataUBO[i].MapMemory(device, &mappedData);
      mem_copy(mappedData, nodes, bufferSize);
      dataUBO[i].UnmapMemory(device);
    }
  }

  // Indices Array
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO = sdfRenderer->indicesUBO;
    const VkDeviceSize bufferSize = sizeof(u32) * indexCount;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = dataUBO[i].Create(device, bufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Vertex Array Buffer %i Creation Failed!", i);
        return;
      }

      void* mappedData;
      dataUBO[i].MapMemory(device, &mappedData);
      mem_copy(mappedData, indices, bufferSize);
      dataUBO[i].UnmapMemory(device);
    }
  }

  // Vertices Array
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO = sdfRenderer->verticesUBO;
    const VkDeviceSize bufferSize = sizeof(Vertex) * mesh->geometryArray[0].vertexCount;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = dataUBO[i].Create(device, bufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Vertex Array Buffer %i Creation Failed!", i);
        return;
      }

      void* mappedData;
      dataUBO[i].MapMemory(device, &mappedData);
      mem_copy(mappedData, mesh->geometryArray[0].vertexArray, bufferSize);
      dataUBO[i].UnmapMemory(device);
    }
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = sdfRenderer->descriptorSets;

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
      // Sdf Data UBO
      VkDescriptorBufferInfo dataBufferInfo = {};
      dataBufferInfo.buffer = sdfRenderer->dataUBO[i].buffer;
      dataBufferInfo.offset = 0;
      dataBufferInfo.range  = sdfRenderer->dataUBO[i].size;

      VkWriteDescriptorSet dataDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      dataDescriptorWrite.dstSet           = descriptorSets[i];
      dataDescriptorWrite.dstBinding       = 0;
      dataDescriptorWrite.dstArrayElement  = 0;
      dataDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      dataDescriptorWrite.descriptorCount  = 1;
      dataDescriptorWrite.pBufferInfo      = &dataBufferInfo;
      dataDescriptorWrite.pImageInfo       = nullptr;
      dataDescriptorWrite.pTexelBufferView = nullptr;

      // BSP Nodes UBO
      VkDescriptorBufferInfo bspNodesBufferInfo = {};
      bspNodesBufferInfo.buffer = sdfRenderer->bspNodesUBO[i].buffer;
      bspNodesBufferInfo.offset = 0;
      bspNodesBufferInfo.range  = sdfRenderer->bspNodesUBO[i].size;

      VkWriteDescriptorSet bspNodesDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      bspNodesDescriptorWrite.dstSet           = descriptorSets[i];
      bspNodesDescriptorWrite.dstBinding       = 1;
      bspNodesDescriptorWrite.dstArrayElement  = 0;
      bspNodesDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bspNodesDescriptorWrite.descriptorCount  = 1;
      bspNodesDescriptorWrite.pBufferInfo      = &bspNodesBufferInfo;
      bspNodesDescriptorWrite.pImageInfo       = nullptr;
      bspNodesDescriptorWrite.pTexelBufferView = nullptr;

      // Index Array UBO
      VkDescriptorBufferInfo indexBufferInfo = {};
      indexBufferInfo.buffer = sdfRenderer->indicesUBO[i].buffer;
      indexBufferInfo.offset = 0;
      indexBufferInfo.range  = sdfRenderer->indicesUBO[i].size;

      VkWriteDescriptorSet indexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      indexDescriptorWrite.dstSet           = descriptorSets[i];
      indexDescriptorWrite.dstBinding       = 2;
      indexDescriptorWrite.dstArrayElement  = 0;
      indexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      indexDescriptorWrite.descriptorCount  = 1;
      indexDescriptorWrite.pBufferInfo      = &indexBufferInfo;
      indexDescriptorWrite.pImageInfo       = nullptr;
      indexDescriptorWrite.pTexelBufferView = nullptr;

      // Vertex Array UBO
      VkDescriptorBufferInfo vertexBufferInfo = {};
      vertexBufferInfo.buffer = sdfRenderer->verticesUBO[i].buffer;
      vertexBufferInfo.offset = 0;
      vertexBufferInfo.range  = sdfRenderer->verticesUBO[i].size;

      VkWriteDescriptorSet vertexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      vertexDescriptorWrite.dstSet           = descriptorSets[i];
      vertexDescriptorWrite.dstBinding       = 3;
      vertexDescriptorWrite.dstArrayElement  = 0;
      vertexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      vertexDescriptorWrite.descriptorCount  = 1;
      vertexDescriptorWrite.pBufferInfo      = &vertexBufferInfo;
      vertexDescriptorWrite.pImageInfo       = nullptr;
      vertexDescriptorWrite.pTexelBufferView = nullptr;

      const VkWriteDescriptorSet descriptorWrites[] =
        {
          dataDescriptorWrite,
          bspNodesDescriptorWrite,
          indexDescriptorWrite,
          vertexDescriptorWrite
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }

  mem_free(nodes);
  mem_free(indices);

  sdfRenderer->boundingBox = mesh->geometryArray[0].CalculateBoundingBox();
  sdfRenderer->indexCount  = indexCount;
}

void ComponentFactory::SdfRendererDestroy(SdfRenderer* sdfRenderer)
{
  const vk::Device& device = Renderer::GetDevice();

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    sdfRenderer->dataUBO[i].UnmapMemory(device);
    sdfRenderer->dataUBO[i].Destroy(device);

    sdfRenderer->bspNodesUBO[i].UnmapMemory(device);
    sdfRenderer->bspNodesUBO[i].Destroy(device);

    sdfRenderer->verticesUBO[i].UnmapMemory(device);
    sdfRenderer->verticesUBO[i].Destroy(device);

    sdfRenderer->indicesUBO[i].UnmapMemory(device);
    sdfRenderer->indicesUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(),
                       MAX_FRAMES_IN_FLIGHT, sdfRenderer->descriptorSets.data);
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create descriptor sets...
  {
    VkDescriptorSetLayoutBinding sdfDataLayoutBinding = {};
    sdfDataLayoutBinding.binding            = 0;
    sdfDataLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sdfDataLayoutBinding.descriptorCount    = 1;
    sdfDataLayoutBinding.pImmutableSamplers = nullptr;
    sdfDataLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bspNodeLayoutBinding = {};
    bspNodeLayoutBinding.binding            = 1;
    bspNodeLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bspNodeLayoutBinding.descriptorCount    = 1;
    bspNodeLayoutBinding.pImmutableSamplers = nullptr;
    bspNodeLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding indexArrayLayoutBinding = {};
    indexArrayLayoutBinding.binding            = 2;
    indexArrayLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexArrayLayoutBinding.descriptorCount    = 1;
    indexArrayLayoutBinding.pImmutableSamplers = nullptr;
    indexArrayLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding vertexArrayLayoutBinding = {};
    vertexArrayLayoutBinding.binding            = 3;
    vertexArrayLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexArrayLayoutBinding.descriptorCount    = 1;
    vertexArrayLayoutBinding.pImmutableSamplers = nullptr;
    vertexArrayLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    const VkDescriptorSetLayoutBinding bindings[] =
      {
        sdfDataLayoutBinding,
        bspNodeLayoutBinding,
        indexArrayLayoutBinding,
        vertexArrayLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  // Load shader code from file
  FileSystem::FileContent vertShaderCode, fragShaderCode;

  if (!FileSystem::ReadAllFileContent("shaders/fullscreen.vert.spv", &vertShaderCode))  ABORT(ABORT_CODE_ASSET_FAILURE);
  if (!FileSystem::ReadAllFileContent("shaders/sdfRenderer.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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
  rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
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

      .renderQueue   = FULL_SCREEN_QUEUE,

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

static void CleanUpPipeline()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<SdfRenderer>();
  for (u32 i = 0; i < sparseSet->componentCount; ++i)
  {
    ComponentData<SdfRenderer>& componentData = ((ComponentData<SdfRenderer>*)sparseSet->componentArray)[i];
    SdfRenderer& renderer = componentData.component;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    SdfDataUBO* dataUbo = (SdfDataUBO*)renderer.dataUBOMapped[currentFrame];
    mem_zero(dataUbo, sizeof(SdfDataUBO));

    dataUbo->WVP         = transform->GetWVPMatrix(camera);
    dataUbo->worldMat    = transform->matrix;
    dataUbo->invWorldMat = glm::inverse(transform->matrix);

    dataUbo->boundingBoxExtents = renderer.boundingBox.GetExtents();
    dataUbo->indexCount         = renderer.indexCount;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &renderer.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // Fullscreen Quad
  }
}
