#include "SdfPointCloudRenderer.hpp"

// core
#include "core/Abort.hpp"
#include "core/Logging.hpp"
#include "core/Random.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// containers
#include "containers/FArray.hpp"
#include <queue>

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// geometry
#include "geometry/BoundingBox3D.hpp"
#include "geometry/PointCloud.hpp"
#include "geometry/Eigen.hpp"
#include "geometry/Plane.hpp"

// math
#include "math/Math.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/ComponentFactory.hpp"
#include "ecs/components/Transform.hpp"

using namespace ECS;

// Forward Declarations
static void CreatePipeline();
static void CleanUpPipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
DArray<glm::vec3> BuildBSPTree(SdfPointCloudRenderer* renderer, const PointCloud* pointCloud);
static INLINE Plane ChooseMaxVarianceSplitPlane(const glm::vec3* points, u32 pointCount);
static FArray<DArray<glm::vec3>, 3> SplitPoints(const glm::vec3* points, u32 pointCount, const Plane& plane);

static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

struct SdfDataUBO
{
  alignas(16) glm::mat4x4 WVP;
  alignas(16) glm::mat4x4 worldMat;
  alignas(16) glm::mat4x4 invWorldMat;
  alignas(16) glm::vec3 boundingBox;

  alignas(4) u32 pointCount;
//  alignas(16) glm::vec3 boundingBoxExtents;
};

void ComponentFactory::SdfPointCloudRendererCreate(SdfPointCloudRenderer* sdfPointCloudRenderer, const PointCloud* pointCloud)
{
  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  LOG_INFO("Building Point Cloud BSP Tree...");
  DArray<glm::vec3> points = BuildBSPTree(sdfPointCloudRenderer, pointCloud);
  sdfPointCloudRenderer->pointCount  = points.Size();
  sdfPointCloudRenderer->boundingBox = pointCloud->CalculateBoundingBox();
  LOG_INFO("Point Cloud BSP Tree Built!");

  const vk::Device& device = Renderer::GetDevice();

  // Sdf Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO       = sdfPointCloudRenderer->dataUBO;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& dataUBOMapped = sdfPointCloudRenderer->dataUBOMapped;

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
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO = sdfPointCloudRenderer->bspNodesUBO;
    const VkDeviceSize bufferSize = sizeof(SdfPointCloudRenderer::BSPNodeUBO) * sdfPointCloudRenderer->nodes.Size();

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
//      mem_zero(mappedData, bufferSize);
      mem_copy(mappedData, sdfPointCloudRenderer->nodes.data, bufferSize);
      dataUBO[i].UnmapMemory(device);
    }
  }

  // Points Array
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataUBO = sdfPointCloudRenderer->pointsUBO;
    const VkDeviceSize bufferSize = sizeof(glm::vec4) * points.Size();

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    glm::vec4* mappedData;
    stagingBuffer.MapMemory(device, (void**)&mappedData);
    for (u32 pointIndex = 0; pointIndex < points.Size(); ++pointIndex)
    {
      const glm::vec3& point = points[pointIndex];
      mappedData[pointIndex] = glm::vec4(point.x, point.y, point.z, 0.0f);
    }
    stagingBuffer.UnmapMemory(device);

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      const b8 success = dataUBO[i].Create(device, bufferSize,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      if (!success)
      {
        LOG_FATAL("UBO Points Array Buffer %i Creation Failed!", i);
        return;
      }

      vk::Buffer::CopyBufferToBuffer(stagingBuffer, dataUBO[i], bufferSize);
    }

    stagingBuffer.Destroy(device);
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = sdfPointCloudRenderer->descriptorSets;

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
      dataBufferInfo.buffer = sdfPointCloudRenderer->dataUBO[i].buffer;
      dataBufferInfo.offset = 0;
      dataBufferInfo.range  = sdfPointCloudRenderer->dataUBO[i].size;

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
      bspNodesBufferInfo.buffer = sdfPointCloudRenderer->bspNodesUBO[i].buffer;
      bspNodesBufferInfo.offset = 0;
      bspNodesBufferInfo.range  = sdfPointCloudRenderer->bspNodesUBO[i].size;

      VkWriteDescriptorSet bspNodesDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      bspNodesDescriptorWrite.dstSet           = descriptorSets[i];
      bspNodesDescriptorWrite.dstBinding       = 1;
      bspNodesDescriptorWrite.dstArrayElement  = 0;
      bspNodesDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bspNodesDescriptorWrite.descriptorCount  = 1;
      bspNodesDescriptorWrite.pBufferInfo      = &bspNodesBufferInfo;
      bspNodesDescriptorWrite.pImageInfo       = nullptr;
      bspNodesDescriptorWrite.pTexelBufferView = nullptr;

      // Point Array UBO
      VkDescriptorBufferInfo pointBufferInfo = {};
      pointBufferInfo.buffer = sdfPointCloudRenderer->pointsUBO[i].buffer;
      pointBufferInfo.offset = 0;
      pointBufferInfo.range  = sdfPointCloudRenderer->pointsUBO[i].size;

      VkWriteDescriptorSet pointDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      pointDescriptorWrite.dstSet           = descriptorSets[i];
      pointDescriptorWrite.dstBinding       = 2;
      pointDescriptorWrite.dstArrayElement  = 0;
      pointDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      pointDescriptorWrite.descriptorCount  = 1;
      pointDescriptorWrite.pBufferInfo      = &pointBufferInfo;
      pointDescriptorWrite.pImageInfo       = nullptr;
      pointDescriptorWrite.pTexelBufferView = nullptr;

      const VkWriteDescriptorSet descriptorWrites[] =
        {
          dataDescriptorWrite,
          bspNodesDescriptorWrite,
          pointDescriptorWrite
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }
}

void ComponentFactory::SdfPointCloudRendererDestroy(SdfPointCloudRenderer* sdfPointCloudRenderer)
{
  const vk::Device& device = Renderer::GetDevice();

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    sdfPointCloudRenderer->dataUBO[i].UnmapMemory(device);
    sdfPointCloudRenderer->dataUBO[i].Destroy(device);

    sdfPointCloudRenderer->bspNodesUBO[i].UnmapMemory(device);
    sdfPointCloudRenderer->bspNodesUBO[i].Destroy(device);

    sdfPointCloudRenderer->pointsUBO[i].UnmapMemory(device);
    sdfPointCloudRenderer->pointsUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(),
                       MAX_FRAMES_IN_FLIGHT, sdfPointCloudRenderer->descriptorSets.data);
}

static void CreatePipeline()
{
  LOG_INFO("Creating SDF Point Cloud Renderer Pipeline...");

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

    VkDescriptorSetLayoutBinding pointArrayLayoutBinding = {};
    pointArrayLayoutBinding.binding            = 2;
    pointArrayLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pointArrayLayoutBinding.descriptorCount    = 1;
    pointArrayLayoutBinding.pImmutableSamplers = nullptr;
    pointArrayLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    const VkDescriptorSetLayoutBinding bindings[] =
      {
        sdfDataLayoutBinding,
        bspNodeLayoutBinding,
        pointArrayLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  // Load shader code from file
  FileSystem::FileContent vertShaderCode, fragShaderCode;

  if (!FileSystem::ReadAllFileContent("shaders/fullscreen.vert.spv", &vertShaderCode))    ABORT(ABORT_CODE_ASSET_FAILURE);
  if (!FileSystem::ReadAllFileContent("shaders/sdfPointCloud.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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

  LOG_INFO("Created SDF Point Cloud Renderer Pipeline!");
}

static void CleanUpPipeline()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<SdfPointCloudRenderer>();
  for (u32 i = 0; i < sparseSet->componentCount; ++i)
  {
    ComponentData<SdfPointCloudRenderer>& componentData = ((ComponentData<SdfPointCloudRenderer>*)sparseSet->componentArray)[i];
    SdfPointCloudRenderer& renderer = componentData.component;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    SdfDataUBO* dataUbo = (SdfDataUBO*)renderer.dataUBOMapped[currentFrame];
    mem_zero(dataUbo, sizeof(SdfDataUBO));

    dataUbo->WVP         = transform->GetWVPMatrix(camera);
    dataUbo->worldMat    = transform->matrix;
    dataUbo->invWorldMat = glm::inverse(transform->matrix);
    dataUbo->boundingBox = renderer.boundingBox.GetExtents();
    dataUbo->pointCount  = renderer.pointCount;

//    dataUbo->boundingBoxExtents = renderer.boundingBox.GetExtents();

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &renderer.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // Fullscreen Quad
  }
}

DArray<glm::vec3> BuildBSPTree(SdfPointCloudRenderer* renderer, const PointCloud* pointCloud)
{
  DArray<glm::vec3> sortedPoints = {};
  sortedPoints.Create(pointCloud->pointCount);

  renderer->nodes.Create(3);
  renderer->nodes.Add({}); // create first node

  struct QueueEntry
  {
    glm::vec3* points;
    u64 pointCount;
    u32 nodeIndex;
    u32 depth;
  };

  std::queue<QueueEntry> queue = {};

  // Insert first queue entry
  queue.push(QueueEntry{
    .points          = pointCloud->points,
    .pointCount      = pointCloud->pointCount,
    .nodeIndex       = 0,
    .depth           = 0
  });

  while(!queue.empty())
  {
    // Get & pop the first element from the queue
    QueueEntry queueEntry = queue.front();
    queue.pop();

    SdfPointCloudRenderer::BSPNodeUBO& thisNode = renderer->nodes[queueEntry.nodeIndex];
    thisNode.pointCount = 0;

    const u32& currentPointCount = queueEntry.pointCount;

    if (currentPointCount <= 0)
    {
//      LOG_DEBUG("Hit empty leaf!");
      thisNode.isLeaf = true;
      continue;
    }

    if (currentPointCount <= 20)
    {
//      LOG_DEBUG("Hit leaf!");
      thisNode.isLeaf     = true;
      thisNode.pointCount = currentPointCount;
      thisNode.pointIndex = sortedPoints.Size();

      PointCloud tempCloud = {};
      tempCloud.points     = queueEntry.points;
      tempCloud.pointCount = currentPointCount;

      BoundingBox3D boundingBox = tempCloud.CalculateBoundingBox();
      thisNode.boundingBox = boundingBox.GetExtents();

      for (u32 i = 0; i < currentPointCount; ++i)
      {
        sortedPoints.Add(queueEntry.points[i]);
      }

      continue;
    }

    thisNode.plane = ChooseMaxVarianceSplitPlane(queueEntry.points, queueEntry.pointCount);

    const FArray<DArray<glm::vec3>, 3> splitPoints = SplitPoints(queueEntry.points, queueEntry.pointCount, thisNode.plane);

    // Add any points that are considered on the plane...
    if (splitPoints[2].Size() > 0)
    {
      thisNode.pointCount = splitPoints[2].Size();
      thisNode.pointIndex = sortedPoints.Size();

      for (u32 i = 0; i < splitPoints[2].Size(); ++i)
      {
        sortedPoints.Add(splitPoints[2][i]);
      }
    }

    // NOTE(WSWhitehouse): MUST NOT USE `thisNode` REFERENCE BEYOND THIS POINT!

    renderer->nodes[queueEntry.nodeIndex].nodePositive = renderer->nodes.Add({});
    queue.push(QueueEntry{
      .points         = splitPoints[0].data,
      .pointCount     = splitPoints[0].Size(),
      .nodeIndex      = renderer->nodes[queueEntry.nodeIndex].nodePositive,
      .depth          = queueEntry.depth + 1
    });

    renderer->nodes[queueEntry.nodeIndex].nodeNegative = renderer->nodes.Add({});
    queue.push(QueueEntry{
      .points         = splitPoints[1].data,
      .pointCount     = splitPoints[1].Size(),
      .nodeIndex      = renderer->nodes[queueEntry.nodeIndex].nodeNegative,
      .depth          = queueEntry.depth + 1
    });
  }

  return sortedPoints;
}

static INLINE Plane ChooseMaxVarianceSplitPlane(const glm::vec3* points, u32 pointCount)
{
  glm::mat3x3 covarianceMatrix = Eigen::CalcCovarianceMatrix3x3(points, pointCount);

  FArray<f32, 3> eigenValues;
  FArray<glm::vec3, 3> eigenVectors;
  Eigen::EigenDecomposition3x3(covarianceMatrix, &eigenValues, &eigenVectors);

  for (u32 i = 0; i < 3; ++i)
  {
    eigenVectors[i] = glm::normalize(eigenVectors[i]);
  }

  glm::vec3 center = { 0.0f, 0.0f, 0.0f };
  for (u64 i = 0; i < pointCount; ++i)
  {
    center += points[i];
  }
  center /= pointCount;

  return Plane
    {
      .position = center,
      .normal   = eigenVectors[2]
    };
}

static FArray<DArray<glm::vec3>, 3> SplitPoints(const glm::vec3* points, u32 pointCount, const Plane& plane)
{
  std::vector<glm::vec3> frontPoints   = {};
  std::vector<glm::vec3> backPoints    = {};
  std::vector<glm::vec3> onPlanePoints = {};

  frontPoints.reserve(pointCount);
  backPoints.reserve(pointCount);
  onPlanePoints.reserve(pointCount);

  for (u64 i = 0; i < pointCount; i++)
  {
    const glm::vec3& point = points[i];

    // Compute the distance to the plane...
    const f32 dist   = plane.SignedDistanceFromPoint(point);
//    const b8 isFront = dist > 0.0f;

    if (dist >= F32_EPSILON)
    {
      frontPoints.push_back(point);
      continue;
    }

    if (dist <= -F32_EPSILON)
    {
      backPoints.push_back(point);
      continue;
    }

    onPlanePoints.push_back(point);
  }

  FArray<DArray<glm::vec3>, 3> outPoints = {};
  outPoints[0].Create(frontPoints.size());
  outPoints[1].Create(backPoints.size());
  outPoints[2].Create(backPoints.size());

  // Front
  {
    DArray<glm::vec3>& front = outPoints[0];
    for (u32 i = 0; i < frontPoints.size(); ++i)
    {
      front.Add(frontPoints[i]);
    }
  }

  // Back
  {
    DArray<glm::vec3>& back = outPoints[1];
    for (u32 i = 0; i < backPoints.size(); ++i)
    {
      back.Add(backPoints[i]);
    }
  }

  // On Plane
  {
    DArray<glm::vec3>& onPlane = outPoints[2];
    for (u32 i = 0; i < onPlanePoints.size(); ++i)
    {
      onPlane.Add(onPlanePoints[i]);
    }
  }

  return outPoints;
};