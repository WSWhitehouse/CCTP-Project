#include "SdfVoxelGrid.hpp"

#include <cmath>

// core
#include "core/Logging.hpp"
#include "filesystem/FileSystem.hpp"
#include "core/Profiler.hpp"

// containers
#include "containers/DArray.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/UniformBufferObjects.hpp"

// geometry
#include "geometry/Mesh.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/BoundingBox3D.hpp"
#include "geometry/Triangle.hpp"

// ecs
#include "ecs/components/Transform.hpp"

using namespace vk;

struct UBOCompTriDistInput
{
  alignas(16) glm::mat4x4 transform;

  // Voxel Grid Data
  alignas(16) glm::uvec4 cellCount;
  alignas(16) glm::vec3 gridOffset;
  alignas(16) glm::vec3 gridExtents;
  alignas(16) glm::vec3 cellSize;
  alignas(04) f32 gridScale;

  // Mesh Geometry Data
  alignas(04) u32 indexCount;
  alignas(04) u32 indexFormat;
};

struct UBOCompJumpFloodInput
{
  alignas(16) glm::uvec4 cellCount;
  alignas(16) glm::vec3 cellSize;
  alignas(04) u32 iteration;
  alignas(04) u32 jumpOffset;
  alignas(04) f32 maxDist;
};

struct UBOActiveCells
{
  alignas(04) u32 count;
  alignas(04) u8 cells[];
};

struct UBOSdfVoxelData
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::mat4 worldMat;
  alignas(16) glm::mat4 invWorldMat;

  alignas(16) glm::uvec3 cellCount;
  alignas(16) glm::vec3 gridExtents;
  alignas(16) glm::vec3 twist;
  alignas(16) glm::vec4 sphere;
  alignas(04) f32 voxelGridScale;
  alignas(04) f32 blend;
  alignas(04) b8 showBounds;
};

STATIC_ASSERT(sizeof(UBOSdfVoxelData) % 16 == 0);

// Compute Pipeline
struct ComputePipeline
{
  VkPipeline pipeline                       = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout           = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet             = VK_NULL_HANDLE;
  Buffer inputBufferUBO                     = {};
};

static ComputePipeline compNaiveDistPipeline    = {};
static ComputePipeline compTriDistPipeline      = {};
static ComputePipeline compJumpFloodingPipeline = {};

// Graphics Pipeline
static PipelineHandle pipelineHandle             = INVALID_PIPELINE_HANDLE;
static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

// Forward Declarations
static b8 CreateNaiveDistComputePipeline();
static b8 CreateTriDistComputePipeline();
static b8 CreateJumpFloodingComputePipeline();

static b8 Create3DImage(SdfVoxelGrid* voxelGrid);

static void DispatchNaiveMethod(SdfVoxelGrid* voxelGrid, const Mesh* mesh);
static void DispatchJumpFloodingMethod(SdfVoxelGrid* voxelGrid, const Mesh* mesh);

static void DispatchNaiveDistComputeShader(SdfVoxelGrid* voxelGrid, const MeshGeometry& geometry, const glm::mat4x4& transform);
static void DispatchTriDistComputeShader(SdfVoxelGrid* voxelGrid, const MeshGeometry& geometry,
                                         const glm::mat4x4& transform, Buffer& activeCells);
static void DispatchJumpFloodingPipeline(SdfVoxelGrid* voxelGrid, Buffer& activeCells);

static void CreatePipeline();
static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
static void CleanUp();

b8 SdfVoxelGrid::CreateComputePipeline()
{
  if (!CreateNaiveDistComputePipeline())    return false;
  if (!CreateTriDistComputePipeline())      return false;
  if (!CreateJumpFloodingComputePipeline()) return false;
  return true;
}

void SdfVoxelGrid::CleanUpComputePipeline()
{
  const Device& device = Renderer::GetDevice();

  // Naive Dist Compute Pipeline
  vkDestroyPipeline(device.logicalDevice, compNaiveDistPipeline.pipeline, nullptr);
  vkDestroyPipelineLayout(device.logicalDevice, compNaiveDistPipeline.pipelineLayout, nullptr);
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), 1, &compNaiveDistPipeline.descriptorSet);
  vkDestroyDescriptorSetLayout(device.logicalDevice, compNaiveDistPipeline.descriptorSetLayout, nullptr);
  compNaiveDistPipeline.inputBufferUBO.Destroy(device);

  // Tri Dist Compute Pipeline
  vkDestroyPipeline(device.logicalDevice, compTriDistPipeline.pipeline, nullptr);
  vkDestroyPipelineLayout(device.logicalDevice, compTriDistPipeline.pipelineLayout, nullptr);
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), 1, &compTriDistPipeline.descriptorSet);
  vkDestroyDescriptorSetLayout(device.logicalDevice, compTriDistPipeline.descriptorSetLayout, nullptr);
  compTriDistPipeline.inputBufferUBO.Destroy(device);

  // Jump Flooding Compute Pipeline
  vkDestroyPipeline(device.logicalDevice, compJumpFloodingPipeline.pipeline, nullptr);
  vkDestroyPipelineLayout(device.logicalDevice, compJumpFloodingPipeline.pipelineLayout, nullptr);
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), 1, &compJumpFloodingPipeline.descriptorSet);
  vkDestroyDescriptorSetLayout(device.logicalDevice, compJumpFloodingPipeline.descriptorSetLayout, nullptr);
  compJumpFloodingPipeline.inputBufferUBO.Destroy(device);
}

void SdfVoxelGrid::Create(SdfVoxelGrid* voxelGrid, b8 useJumpFlooding, const Mesh* mesh, glm::uvec3 uCellCount)
{
  LOG_INFO("SdfVoxelGrid: Creating Voxel Grid from Mesh...");

  if (pipelineHandle == INVALID_PIPELINE_HANDLE) CreatePipeline();

  const vk::Device& device = Renderer::GetDevice();

  // Calculate Voxel Grid Values
  {
    // Calculate the bounding box for the entire mesh
    BoundingBox3D boundingBox
      {
        .minimum = {F32_MAX, F32_MAX, F32_MAX},
        .maximum = {F32_MIN, F32_MIN, F32_MIN}
      };

    for (u32 i = 0; i < mesh->nodeCount; ++i)
    {
      const MeshNode& node         = mesh->nodeArray[i];
      const MeshGeometry& geometry = mesh->geometryArray[node.geometryIndex];

      const BoundingBox3D geometryBoundingBox = geometry.CalculateBoundingBox(node.transformMatrix);
      boundingBox = BoundingBox3D::Combine(boundingBox, geometryBoundingBox);
    }

    // Grid Extent
    const glm::vec3 meshExtent  = boundingBox.GetSize();
    const f32 maxExtentValue    = MAX(meshExtent.x, MAX(meshExtent.y, meshExtent.z));
    const glm::vec3 gridExtent  = glm::vec3(maxExtentValue, maxExtentValue, maxExtentValue);
    voxelGrid->gridExtent       = gridExtent;
    voxelGrid->gridCenterOffset = boundingBox.GetCenter();

    // Cell Count
    const glm::vec3 cellCount = glm::vec3(uCellCount);
    const u64 totalCellCount  = uCellCount.x * uCellCount.y * uCellCount.z;
    voxelGrid->cellCount      = glm::uvec4(uCellCount.x, uCellCount.y, uCellCount.z, totalCellCount);

    // Scaling Factor
    const glm::vec3 cellSize      = gridExtent / cellCount;
    const glm::vec3 scalingFactor = cellCount / (gridExtent + cellSize * 2.0f);
    voxelGrid->scalingFactor      = MIN(scalingFactor.x, MIN(scalingFactor.y, scalingFactor.z));

    // Cell Size
    voxelGrid->cellSize = (gridExtent / cellCount) * scalingFactor;
  }

  const u64 memorySize = sizeof(f32) * voxelGrid->cellCount.w;
  LOG_DEBUG("Grid Memory Size (MiB): %i", memorySize / 1024 / 1024);

  if (!Create3DImage(voxelGrid))
  {
    mem_free(voxelGrid);
    return;
  }

  if (useJumpFlooding)
  {
    DispatchJumpFloodingMethod(voxelGrid, mesh);
  }
  else
  {
    DispatchNaiveMethod(voxelGrid, mesh);
  }

  // SDF Voxel Data
  {
    FArray<vk::Buffer, MAX_FRAMES_IN_FLIGHT>& dataBuffer = voxelGrid->dataUniformBuffer;
    FArray<void*,      MAX_FRAMES_IN_FLIGHT>& dataMapped = voxelGrid->dataUniformBuffersMapped;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      b8 success = dataBuffer[i].Create(device, sizeof(UBOSdfVoxelData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success)
      {
        LOG_FATAL("UBO SDF Voxel Data Buffer %i Creation Failed!", i);
        return; // TODO(WSWhitehouse): fix the memory leak here.
      }

      dataBuffer[i].MapMemory(device, &dataMapped[i]);
    }
  }

  // Create Descriptor Sets
  {
    FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& descriptorSets = voxelGrid->descriptorSets;

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
      // Sdf Voxel Data UBO
      VkDescriptorBufferInfo dataBufferInfo = {};
      dataBufferInfo.buffer = voxelGrid->dataUniformBuffer[i].buffer;
      dataBufferInfo.offset = 0;
      dataBufferInfo.range  = sizeof(UBOSdfVoxelData);

      VkWriteDescriptorSet dataDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      dataDescriptorWrite.dstSet           = descriptorSets[i];
      dataDescriptorWrite.dstBinding       = 0;
      dataDescriptorWrite.dstArrayElement  = 0;
      dataDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      dataDescriptorWrite.descriptorCount  = 1;
      dataDescriptorWrite.pBufferInfo      = &dataBufferInfo;
      dataDescriptorWrite.pImageInfo       = nullptr;
      dataDescriptorWrite.pTexelBufferView = nullptr;

      VkDescriptorImageInfo voxelGridInfo = {};
      voxelGridInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      voxelGridInfo.imageView   = voxelGrid->imageView;
      voxelGridInfo.sampler     = voxelGrid->imageSampler;

      VkWriteDescriptorSet voxelGridDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      voxelGridDescriptorWrite.dstSet          = descriptorSets[i];
      voxelGridDescriptorWrite.dstBinding      = 1;
      voxelGridDescriptorWrite.dstArrayElement = 0;
      voxelGridDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      voxelGridDescriptorWrite.descriptorCount = 1;
      voxelGridDescriptorWrite.pImageInfo      = &voxelGridInfo;

      VkWriteDescriptorSet descriptorWrites[] =
        {
          dataDescriptorWrite,
          voxelGridDescriptorWrite
        };

      vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
    }
  }

  LOG_INFO("SdfVoxelGrid: Volume Created!");
}

void SdfVoxelGrid::Release(SdfVoxelGrid* sdfVolume)
{
  const Device& device = Renderer::GetDevice();

  vkDestroySampler(device.logicalDevice, sdfVolume->imageSampler, nullptr);
  vkDestroyImageView(device.logicalDevice, sdfVolume->imageView, nullptr);
  sdfVolume->image.Destroy(device);

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(),
                       MAX_FRAMES_IN_FLIGHT, sdfVolume->descriptorSets.data);

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    sdfVolume->dataUniformBuffer[i].Destroy(device);
  }
}

static b8 Create3DImage(SdfVoxelGrid* voxelGrid)
{
  LOG_INFO("SdfVoxelGrid: Creating Voxel Image...");

  const Device& device = Renderer::GetDevice();

  // Create image
  {
    const VkExtent3D extent =
      {
        .width  = voxelGrid->cellCount.x,
        .height = voxelGrid->cellCount.y,
        .depth  = voxelGrid->cellCount.z
      };

    const u32 queueFamilyIndices[] =
      {
        device.queueFamilyIndices.graphicsFamily,
        device.queueFamilyIndices.computeFamily,
      };

    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType             = VK_IMAGE_TYPE_3D;
    imageCreateInfo.mipLevels             = 1;
    imageCreateInfo.arrayLayers           = 1;
    imageCreateInfo.extent                = extent;
    imageCreateInfo.format                = VK_FORMAT_R32_SFLOAT;
    imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage                 = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
    imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags                 = 0;
    imageCreateInfo.queueFamilyIndexCount = ARRAY_SIZE(queueFamilyIndices);
    imageCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;

    if (!voxelGrid->image.Create(device, &imageCreateInfo))
    {
      LOG_ERROR("Failed to create volume image!");
      return false;
    }
  }

  // NOTE(WSWhitehouse): This is used later to initialise the image and when creating the image view...
  const VkImageSubresourceRange subresourceRange =
    {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1
    };

  // Create the image view
  {
    VkImageViewCreateInfo viewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewCreateInfo.image            = voxelGrid->image.image;
    viewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_3D;
    viewCreateInfo.format           = VK_FORMAT_R32_SFLOAT;
    viewCreateInfo.subresourceRange = subresourceRange;

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &viewCreateInfo, nullptr, &voxelGrid->imageView));
  }

  // Create the image sampler
  {
    VkSamplerCreateInfo samplerCreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerCreateInfo.anisotropyEnable        = VK_FALSE;
    samplerCreateInfo.maxAnisotropy           = device.properties.limits.maxSamplerAnisotropy;
    samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.mipLodBias              = 0.0f;
    samplerCreateInfo.minLod                  = 0.0f;
    samplerCreateInfo.maxLod                  = 0.0f;

    VK_SUCCESS_CHECK(vkCreateSampler(device.logicalDevice, &samplerCreateInfo, nullptr, &voxelGrid->imageSampler));
  }

  // Set initial image values and transition layout ready for compute shader
  {
    const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();
    VkCommandBuffer cmdBuffer      = cmdPool.SingleTimeCommandBegin(device);

    vk::Image::CmdTransitionBarrier(cmdBuffer, voxelGrid->image.image,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    subresourceRange);

    VkClearColorValue initialColour = {F32_MAX, F32_MAX, F32_MAX, F32_MAX};
    vkCmdClearColorImage(cmdBuffer, voxelGrid->image.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &initialColour, 1, &subresourceRange);

    vk::Image::CmdTransitionBarrier(cmdBuffer, voxelGrid->image.image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    subresourceRange);

    cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
  }

  LOG_INFO("SdfVoxelGrid: Voxel Image Created!");
  return true;
}

static void DispatchNaiveMethod(SdfVoxelGrid* voxelGrid, const Mesh* mesh)
{
  PROFILE_FUNC

  for (u32 i = 0; i < mesh->nodeCount; ++i)
  {
    const MeshNode& node         = mesh->nodeArray[i];
    const MeshGeometry& geometry = mesh->geometryArray[node.geometryIndex];
    DispatchNaiveDistComputeShader(voxelGrid, geometry, node.transformMatrix);
  }

  // NOTE(WSWhitehouse): Must transition the image to a layout optimal for shader reads
  // TODO(WSWhitehouse): Transition layout needs appropriate queue family indices... this should crash.
  vk::Image::TransitionLayout(voxelGrid->image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void DispatchJumpFloodingMethod(SdfVoxelGrid* voxelGrid, const Mesh* mesh)
{
  PROFILE_FUNC

  const Device& device = Renderer::GetDevice();

  Buffer activeCells = {};

  // Create Active Cells buffer
  {
    const VkDeviceSize bufferSize = sizeof(u32) + (sizeof(u8) * voxelGrid->cellCount.w);

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    stagingBuffer.MapMemory(device, &data);
    mem_zero(data, bufferSize);
    stagingBuffer.UnmapMemory(device);

    activeCells.Create(device, bufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT   |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vk::Buffer::CopyBufferToBuffer(stagingBuffer, activeCells, bufferSize);

    stagingBuffer.Destroy(device);
  }

  for (u32 i = 0; i < mesh->nodeCount; ++i)
  {
    const MeshNode& node         = mesh->nodeArray[i];
    const MeshGeometry& geometry = mesh->geometryArray[node.geometryIndex];
    DispatchTriDistComputeShader(voxelGrid, geometry, node.transformMatrix, activeCells);
  }

  DispatchJumpFloodingPipeline(voxelGrid, activeCells);

  // NOTE(WSWhitehouse): Must transition the image to a layout optimal for shader reads
  // TODO(WSWhitehouse): Transition layout needs appropriate queue family indices... this should crash.
  vk::Image::TransitionLayout(voxelGrid->image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Clean Up...
  {
    activeCells.Destroy(device);
  }
}

static void DispatchNaiveDistComputeShader(SdfVoxelGrid* voxelGrid, const MeshGeometry& geometry, const glm::mat4x4& transform)
{
  // References
  const Device& device = Renderer::GetDevice();

  // Compute Buffers
  Buffer vertexBuffer = {};
  Buffer indexBuffer  = {};

  // Update input buffer
  {
    const UBOCompTriDistInput voxelGridInput =
      {
        .transform = transform,

        .cellCount   = voxelGrid->cellCount,
        .gridOffset  = voxelGrid->gridCenterOffset,
        .gridExtents = voxelGrid->gridExtent,
        .cellSize    = voxelGrid->cellSize,
        .gridScale   = voxelGrid->scalingFactor,

        .indexCount  = (u32)geometry.indexCount,
        .indexFormat = (u32)geometry.indexType,
      };

    void* data;
    compNaiveDistPipeline.inputBufferUBO.MapMemory(device, &data);
    mem_copy(data, &voxelGridInput, compNaiveDistPipeline.inputBufferUBO.size);
    compNaiveDistPipeline.inputBufferUBO.UnmapMemory(device);
  }

  // Create Vertex buffer
  {
    const VkDeviceSize bufferSize = sizeof(Vertex) * geometry.vertexCount;

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    stagingBuffer.MapMemory(device, &data);
    mem_copy(data, geometry.vertexArray, bufferSize);
    stagingBuffer.UnmapMemory(device);

    vertexBuffer.Create(device, bufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk::Buffer::CopyBufferToBuffer(stagingBuffer, vertexBuffer, bufferSize);

    stagingBuffer.Destroy(device);
  }

  // Create Index buffer
  {
    const VkDeviceSize bufferSize = geometry.SizeOfIndex() * geometry.indexCount;

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    stagingBuffer.MapMemory(device, &data);
    mem_copy(data, geometry.indexArray, bufferSize);
    stagingBuffer.UnmapMemory(device);

    indexBuffer.Create(device, bufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk::Buffer::CopyBufferToBuffer(stagingBuffer, indexBuffer, bufferSize);

    stagingBuffer.Destroy(device);
  }

  // Update descriptor sets
  {
    // Vertex Buffer
    VkDescriptorBufferInfo vertexBufferInfo = {};
    vertexBufferInfo.buffer = vertexBuffer.buffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range  = vertexBuffer.size;

    VkWriteDescriptorSet vertexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    vertexDescriptorWrite.dstSet           = compNaiveDistPipeline.descriptorSet;
    vertexDescriptorWrite.dstBinding       = 1;
    vertexDescriptorWrite.dstArrayElement  = 0;
    vertexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexDescriptorWrite.descriptorCount  = 1;
    vertexDescriptorWrite.pBufferInfo      = &vertexBufferInfo;
    vertexDescriptorWrite.pImageInfo       = nullptr;
    vertexDescriptorWrite.pTexelBufferView = nullptr;

    // Index Buffer
    VkDescriptorBufferInfo indexBufferInfo = {};
    indexBufferInfo.buffer = indexBuffer.buffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range  = indexBuffer.size;

    VkWriteDescriptorSet indexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    indexDescriptorWrite.dstSet           = compNaiveDistPipeline.descriptorSet;
    indexDescriptorWrite.dstBinding       = 2;
    indexDescriptorWrite.dstArrayElement  = 0;
    indexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexDescriptorWrite.descriptorCount  = 1;
    indexDescriptorWrite.pBufferInfo      = &indexBufferInfo;
    indexDescriptorWrite.pImageInfo       = nullptr;
    indexDescriptorWrite.pTexelBufferView = nullptr;

    // Voxel Volume Image
    VkDescriptorImageInfo voxelImageInfo = {};
    voxelImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    voxelImageInfo.imageView   = voxelGrid->imageView;
    voxelImageInfo.sampler     = nullptr;

    VkWriteDescriptorSet voxelDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    voxelDescriptorWrite.dstSet           = compNaiveDistPipeline.descriptorSet;
    voxelDescriptorWrite.dstBinding       = 3;
    voxelDescriptorWrite.dstArrayElement  = 0;
    voxelDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelDescriptorWrite.descriptorCount  = 1;
    voxelDescriptorWrite.pBufferInfo      = nullptr;
    voxelDescriptorWrite.pImageInfo       = &voxelImageInfo;
    voxelDescriptorWrite.pTexelBufferView = nullptr;

    VkWriteDescriptorSet descriptorWrites[] =
      {
        vertexDescriptorWrite,
        indexDescriptorWrite,
        voxelDescriptorWrite,
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }

  // Dispatch compute shader
  {
    LOG_INFO("SdfVoxelGrid: Dispatching Compute Shader...");

    const CommandPool& cmdPool = Renderer::GetComputeCommandPool();
    VkCommandBuffer cmdBuffer  = cmdPool.SingleTimeCommandBegin(device);

    // Bind resources and pipeline
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compNaiveDistPipeline.pipelineLayout, 0, 1,
                            &compNaiveDistPipeline.descriptorSet, 0, nullptr);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compNaiveDistPipeline.pipeline);

    // Dispatch
    constexpr const glm::uvec3 localSize = { 64, 1, 1 };
    const u32 groupCountX = voxelGrid->cellCount.w / localSize.x;
    vkCmdDispatch(cmdBuffer, groupCountX, 1, 1);

    cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
    LOG_INFO("SdfVoxelGrid: Compute Shader Finished!");
  }

  // Cleanup
  {
    vertexBuffer.Destroy(device);
    indexBuffer.Destroy(device);
  }
}

static void DispatchTriDistComputeShader(SdfVoxelGrid* voxelGrid, const MeshGeometry& geometry,
                                         const glm::mat4x4& transform, Buffer& activeCells)
{
  // References
  const Device& device = Renderer::GetDevice();

  // Compute Buffers
  Buffer vertexBuffer = {};
  Buffer indexBuffer  = {};

  // Update input buffer
  {
    const UBOCompTriDistInput voxelGridInput =
    {
      .transform = transform,

      .cellCount   = voxelGrid->cellCount,
      .gridOffset  = voxelGrid->gridCenterOffset,
      .gridExtents = voxelGrid->gridExtent,
      .cellSize    = voxelGrid->cellSize,
      .gridScale   = voxelGrid->scalingFactor,

      .indexCount  = (u32)geometry.indexCount,
      .indexFormat = (u32)geometry.indexType,
    };

    void* data;
    compTriDistPipeline.inputBufferUBO.MapMemory(device, &data);
    mem_copy(data, &voxelGridInput, compTriDistPipeline.inputBufferUBO.size);
    compTriDistPipeline.inputBufferUBO.UnmapMemory(device);
  }

  // Create Vertex buffer
  {
    const VkDeviceSize bufferSize = sizeof(Vertex) * geometry.vertexCount;

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    stagingBuffer.MapMemory(device, &data);
    mem_copy(data, geometry.vertexArray, bufferSize);
    stagingBuffer.UnmapMemory(device);

    vertexBuffer.Create(device, bufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk::Buffer::CopyBufferToBuffer(stagingBuffer, vertexBuffer, bufferSize);

    stagingBuffer.Destroy(device);
  }

  // Create Index buffer
  {
    const VkDeviceSize bufferSize = geometry.SizeOfIndex() * geometry.indexCount;

    vk::Buffer stagingBuffer = {};
    stagingBuffer.Create(device, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    stagingBuffer.MapMemory(device, &data);
    mem_copy(data, geometry.indexArray, bufferSize);
    stagingBuffer.UnmapMemory(device);

    indexBuffer.Create(device, bufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk::Buffer::CopyBufferToBuffer(stagingBuffer, indexBuffer, bufferSize);

    stagingBuffer.Destroy(device);
  }

  // Update descriptor sets
  {
    // Vertex Buffer
    VkDescriptorBufferInfo vertexBufferInfo = {};
    vertexBufferInfo.buffer = vertexBuffer.buffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range  = vertexBuffer.size;

    VkWriteDescriptorSet vertexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    vertexDescriptorWrite.dstSet           = compTriDistPipeline.descriptorSet;
    vertexDescriptorWrite.dstBinding       = 1;
    vertexDescriptorWrite.dstArrayElement  = 0;
    vertexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexDescriptorWrite.descriptorCount  = 1;
    vertexDescriptorWrite.pBufferInfo      = &vertexBufferInfo;
    vertexDescriptorWrite.pImageInfo       = nullptr;
    vertexDescriptorWrite.pTexelBufferView = nullptr;

    // Index Buffer
    VkDescriptorBufferInfo indexBufferInfo = {};
    indexBufferInfo.buffer = indexBuffer.buffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range  = indexBuffer.size;

    VkWriteDescriptorSet indexDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    indexDescriptorWrite.dstSet           = compTriDistPipeline.descriptorSet;
    indexDescriptorWrite.dstBinding       = 2;
    indexDescriptorWrite.dstArrayElement  = 0;
    indexDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexDescriptorWrite.descriptorCount  = 1;
    indexDescriptorWrite.pBufferInfo      = &indexBufferInfo;
    indexDescriptorWrite.pImageInfo       = nullptr;
    indexDescriptorWrite.pTexelBufferView = nullptr;

    // Voxel Volume Image
    VkDescriptorImageInfo voxelImageInfo = {};
    voxelImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    voxelImageInfo.imageView   = voxelGrid->imageView;
    voxelImageInfo.sampler     = nullptr;

    VkWriteDescriptorSet voxelDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    voxelDescriptorWrite.dstSet           = compTriDistPipeline.descriptorSet;
    voxelDescriptorWrite.dstBinding       = 3;
    voxelDescriptorWrite.dstArrayElement  = 0;
    voxelDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelDescriptorWrite.descriptorCount  = 1;
    voxelDescriptorWrite.pBufferInfo      = nullptr;
    voxelDescriptorWrite.pImageInfo       = &voxelImageInfo;
    voxelDescriptorWrite.pTexelBufferView = nullptr;

    // Active Cells Buffer
    VkDescriptorBufferInfo activeCellsBufferInfo = {};
    activeCellsBufferInfo.buffer = activeCells.buffer;
    activeCellsBufferInfo.offset = 0;
    activeCellsBufferInfo.range  = activeCells.size;

    VkWriteDescriptorSet activeCellsDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    activeCellsDescriptorWrite.dstSet           = compTriDistPipeline.descriptorSet;
    activeCellsDescriptorWrite.dstBinding       = 4;
    activeCellsDescriptorWrite.dstArrayElement  = 0;
    activeCellsDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCellsDescriptorWrite.descriptorCount  = 1;
    activeCellsDescriptorWrite.pBufferInfo      = &activeCellsBufferInfo;
    activeCellsDescriptorWrite.pImageInfo       = nullptr;
    activeCellsDescriptorWrite.pTexelBufferView = nullptr;

    VkWriteDescriptorSet descriptorWrites[] =
    {
      vertexDescriptorWrite,
      indexDescriptorWrite,
      voxelDescriptorWrite,
      activeCellsDescriptorWrite,
    };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }

  // Dispatch compute shader
  {
    LOG_INFO("SdfVoxelGrid: Dispatching Compute Shader...");

    const CommandPool& cmdPool = Renderer::GetComputeCommandPool();
    VkCommandBuffer cmdBuffer  = cmdPool.SingleTimeCommandBegin(device);

    // Bind resources and pipeline
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compTriDistPipeline.pipelineLayout, 0, 1, &compTriDistPipeline.descriptorSet, 0, nullptr);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compTriDistPipeline.pipeline);

    // Dispatch
    constexpr const glm::uvec3 localSize = { 64, 1, 1 };
    const u32 groupCountX = (geometry.indexCount / 3) / localSize.x;
    vkCmdDispatch(cmdBuffer, groupCountX, 1, 1);

    cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
    LOG_INFO("SdfVoxelGrid: Compute Shader Finished!");
  }

  // Cleanup
  {
    vertexBuffer.Destroy(device);
    indexBuffer.Destroy(device);
  }
}

static void DispatchJumpFloodingPipeline(SdfVoxelGrid* voxelGrid, Buffer& activeCells)
{
  LOG_INFO("SdfVoxelGrid: Starting Jump Flooding...");

  const vk::Device& device = Renderer::GetDevice();

  Buffer updatedCells = {};

  // Create Updated Cells Buffer...
  {
    updatedCells.Create(device, activeCells.size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT   |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer::CopyBufferToBuffer(activeCells, updatedCells, activeCells.size);
  }

  // Update descriptor sets
  {
    // Active Cells Buffer
    VkDescriptorBufferInfo activeCellsBufferInfo = {};
    activeCellsBufferInfo.buffer = activeCells.buffer;
    activeCellsBufferInfo.offset = 0;
    activeCellsBufferInfo.range  = activeCells.size;

    VkWriteDescriptorSet activeCellsDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    activeCellsDescriptorWrite.dstSet           = compJumpFloodingPipeline.descriptorSet;
    activeCellsDescriptorWrite.dstBinding       = 1;
    activeCellsDescriptorWrite.dstArrayElement  = 0;
    activeCellsDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCellsDescriptorWrite.descriptorCount  = 1;
    activeCellsDescriptorWrite.pBufferInfo      = &activeCellsBufferInfo;
    activeCellsDescriptorWrite.pImageInfo       = nullptr;
    activeCellsDescriptorWrite.pTexelBufferView = nullptr;

    // Updated Cells Buffer
    VkDescriptorBufferInfo updatedCellsBufferInfo = {};
    updatedCellsBufferInfo.buffer = updatedCells.buffer;
    updatedCellsBufferInfo.offset = 0;
    updatedCellsBufferInfo.range  = updatedCells.size;

    VkWriteDescriptorSet updatedCellsDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    updatedCellsDescriptorWrite.dstSet           = compJumpFloodingPipeline.descriptorSet;
    updatedCellsDescriptorWrite.dstBinding       = 2;
    updatedCellsDescriptorWrite.dstArrayElement  = 0;
    updatedCellsDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    updatedCellsDescriptorWrite.descriptorCount  = 1;
    updatedCellsDescriptorWrite.pBufferInfo      = &updatedCellsBufferInfo;
    updatedCellsDescriptorWrite.pImageInfo       = nullptr;
    updatedCellsDescriptorWrite.pTexelBufferView = nullptr;

    // Voxel Volume Image
    VkDescriptorImageInfo voxelImageInfo = {};
    voxelImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    voxelImageInfo.imageView   = voxelGrid->imageView;
    voxelImageInfo.sampler     = nullptr;

    VkWriteDescriptorSet voxelDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    voxelDescriptorWrite.dstSet           = compJumpFloodingPipeline.descriptorSet;
    voxelDescriptorWrite.dstBinding       = 3;
    voxelDescriptorWrite.dstArrayElement  = 0;
    voxelDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelDescriptorWrite.descriptorCount  = 1;
    voxelDescriptorWrite.pBufferInfo      = nullptr;
    voxelDescriptorWrite.pImageInfo       = &voxelImageInfo;
    voxelDescriptorWrite.pTexelBufferView = nullptr;

    const VkWriteDescriptorSet descriptorWrites[] =
      {
        activeCellsDescriptorWrite,
        updatedCellsDescriptorWrite,
        voxelDescriptorWrite,
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }

  const CommandPool& cmdPool = Renderer::GetComputeCommandPool();

  // Iterations
  const glm::vec3& cellCount = glm::vec3(voxelGrid->cellCount.x, voxelGrid->cellCount.y, voxelGrid->cellCount.z);
  const u32 numIterations    = (u32)glm::ceil(glm::log2(glm::max(cellCount.x, glm::max(cellCount.y, cellCount.z)))) + 1;

  // Group Count
//  constexpr const glm::uvec3 localSize = {64, 1, 1};
//  const u32 groupCountX = cellCount.w / localSize.x;
  constexpr const glm::uvec3 localSize = {8, 8, 8};
  const glm::uvec3 groupCount = glm::uvec3(cellCount) / localSize;

  for (u32 i = 0; i < numIterations; ++i)
  {
    const u32 jumpOffset = numIterations - i;// (u32)(glm::pow(2, numIterations - i));

    // Update input buffer
    {
      const UBOCompJumpFloodInput voxelGridInput =
        {
          .cellCount  = voxelGrid->cellCount,
          .cellSize   = voxelGrid->cellSize,
          .iteration  = numIterations - i,
          .jumpOffset = jumpOffset,
          .maxDist    = F32_MAX
        };

      void* data;
      compJumpFloodingPipeline.inputBufferUBO.MapMemory(device, &data);
      mem_copy(data, &voxelGridInput, compJumpFloodingPipeline.inputBufferUBO.size);
      compJumpFloodingPipeline.inputBufferUBO.UnmapMemory(device);
    }

    // Dispatch Compute Shader...
    {
      VkCommandBuffer cmdBuffer = cmdPool.SingleTimeCommandBegin(device);

      // Bind resources and pipeline
      vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compJumpFloodingPipeline.pipelineLayout,
                              0, 1, &compJumpFloodingPipeline.descriptorSet, 0, nullptr);
      vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compJumpFloodingPipeline.pipeline);

      // Dispatch
      vkCmdDispatch(cmdBuffer, groupCount.x, groupCount.y, groupCount.z);

      cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
    }

    // Update active cells
    Buffer::CopyBufferToBuffer(updatedCells, activeCells, activeCells.size);
  }

  // Clean Up...
  {
    updatedCells.Destroy(device);
  }

  LOG_INFO("SdfVoxelGrid: Jump Flooding Complete!");
}

static void CreatePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create descriptor sets...
  {
    VkDescriptorSetLayoutBinding sdfVoxelDataLayoutBinding = {};
    sdfVoxelDataLayoutBinding.binding            = 0;
    sdfVoxelDataLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sdfVoxelDataLayoutBinding.descriptorCount    = 1;
    sdfVoxelDataLayoutBinding.pImmutableSamplers = nullptr;
    sdfVoxelDataLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding voxelGridLayoutBinding = {};
    voxelGridLayoutBinding.binding            = 1;
    voxelGridLayoutBinding.descriptorCount    = 1;
    voxelGridLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    voxelGridLayoutBinding.pImmutableSamplers = nullptr;
    voxelGridLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        sdfVoxelDataLayoutBinding,
        voxelGridLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  }

  // Load shader code from file
  FileSystem::FileContent vertShaderCode, fragShaderCode;

  if (!FileSystem::ReadAllFileContent("shaders/fullscreen.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
  if (!FileSystem::ReadAllFileContent("shaders/sdfvoxelgrid/voxelRaymarch.frag.spv", &fragShaderCode))   ABORT(ABORT_CODE_ASSET_FAILURE);

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
      .cleanUpFuncPtr = CleanUp,

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

static void Render(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  using namespace ECS;

  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(pipelineHandle);

  ComponentSparseSet* voxelGridSparseSet = ecs.GetComponentSparseSet<SdfVoxelGrid>();
  for (u32 voxelGridIndex = 0; voxelGridIndex < voxelGridSparseSet->componentCount; ++voxelGridIndex)
  {
    ComponentData<SdfVoxelGrid>& componentData = ((ComponentData<SdfVoxelGrid>*)voxelGridSparseSet->componentArray)[voxelGridIndex];
    SdfVoxelGrid& voxelGrid = componentData.component;

    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    ImGui::Begin("SDF Voxel Grid");
    ImGui::InputFloat3("Position", glm::value_ptr(transform->position));
    ImGui::InputFloat3("Rotation", glm::value_ptr(transform->rotation));
    ImGui::InputFloat3("Scale", glm::value_ptr(transform->scale));
    ImGui::InputFloat3("Twist", glm::value_ptr(voxelGrid.twist));
    ImGui::InputFloat3("Sphere Pos", glm::value_ptr(voxelGrid.sphere));
    ImGui::InputFloat("Sphere Radius", &voxelGrid.sphere.w);
    ImGui::InputFloat("Sphere Blend", &voxelGrid.sphereBlend);
    ImGui::Checkbox("Show Bounds", &voxelGrid.showBounds);
    ImGui::End();

    UBOSdfVoxelData* data = (UBOSdfVoxelData*)voxelGrid.dataUniformBuffersMapped[currentFrame];
    data->WVP            = transform->GetWVPMatrix(camera);
    data->worldMat       = transform->matrix;
    data->invWorldMat    = glm::inverse(transform->matrix);
    data->cellCount      = voxelGrid.cellCount;
    data->gridExtents    = voxelGrid.gridExtent;
    data->twist          = voxelGrid.twist;
    data->sphere         = voxelGrid.sphere;
    data->voxelGridScale = voxelGrid.scalingFactor;
    data->blend          = voxelGrid.sphereBlend;
    data->showBounds     = voxelGrid.showBounds;

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                            1, 1, &voxelGrid.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // Fullscreen Quad
  }
}

static void CleanUp()
{
  const vk::Device& device = Renderer::GetDevice();
  vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayout, nullptr);
}

static b8 CreateNaiveDistComputePipeline()
{
  const Device& device = Renderer::GetDevice();

  // Create descriptor set layout
  {
    VkDescriptorSetLayoutBinding inputBufferLayoutBinding = {};
    inputBufferLayoutBinding.binding            = 0;
    inputBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputBufferLayoutBinding.descriptorCount    = 1;
    inputBufferLayoutBinding.pImmutableSamplers = nullptr;
    inputBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding vertexBufferLayoutBinding = {};
    vertexBufferLayoutBinding.binding            = 1;
    vertexBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBufferLayoutBinding.descriptorCount    = 1;
    vertexBufferLayoutBinding.pImmutableSamplers = nullptr;
    vertexBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding indexBufferLayoutBinding = {};
    indexBufferLayoutBinding.binding            = 2;
    indexBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBufferLayoutBinding.descriptorCount    = 1;
    indexBufferLayoutBinding.pImmutableSamplers = nullptr;
    indexBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelGridLayoutBinding = {};
    voxelGridLayoutBinding.binding            = 3;
    voxelGridLayoutBinding.descriptorCount    = 1;
    voxelGridLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelGridLayoutBinding.pImmutableSamplers = nullptr;
    voxelGridLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding activeCellsBufferLayoutBinding = {};
    activeCellsBufferLayoutBinding.binding            = 4;
    activeCellsBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCellsBufferLayoutBinding.descriptorCount    = 1;
    activeCellsBufferLayoutBinding.pImmutableSamplers = nullptr;
    activeCellsBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        inputBufferLayoutBinding,
        vertexBufferLayoutBinding,
        indexBufferLayoutBinding,
        voxelGridLayoutBinding,
        activeCellsBufferLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &compNaiveDistPipeline.descriptorSetLayout));
  }

  // Create Descriptor Sets
  {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool     = Renderer::GetDescriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts        = &compNaiveDistPipeline.descriptorSetLayout;

    VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice, &descriptorSetAllocateInfo, &compNaiveDistPipeline.descriptorSet));
  }

  // Create pipeline layout
  {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &compNaiveDistPipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &compNaiveDistPipeline.pipelineLayout));
  }

  // Create compute pipeline
  {
    // Read compute shader code
    FileSystem::FileContent computeShaderCode;
    if (!FileSystem::ReadAllFileContent("shaders/sdfvoxelgrid/computeNaiveDist.comp.spv", &computeShaderCode))
    {
      LOG_ERROR("Unable to load compute shader code!");
      return false;
    }

    VkShaderModule computeShaderModule = CreateShaderModule(device, computeShaderCode, nullptr);

    VkPipelineShaderStageCreateInfo computeShaderCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    computeShaderCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderCreateInfo.module = computeShaderModule;
    computeShaderCreateInfo.pName  = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computePipelineCreateInfo.stage  = computeShaderCreateInfo;
    computePipelineCreateInfo.layout = compNaiveDistPipeline.pipelineLayout;

    VK_SUCCESS_CHECK(vkCreateComputePipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                              &computePipelineCreateInfo, nullptr, &compNaiveDistPipeline.pipeline));

    vkDestroyShaderModule(device.logicalDevice, computeShaderModule, nullptr);
  }

  // Create UBO voxel grid input buffer
  {
    VkDeviceSize bufferSize = sizeof(UBOCompTriDistInput);
    compNaiveDistPipeline.inputBufferUBO.Create(device, bufferSize,
                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    // Update descriptor sets
    VkDescriptorBufferInfo inputBufferInfo = {};
    inputBufferInfo.buffer = compNaiveDistPipeline.inputBufferUBO.buffer;
    inputBufferInfo.offset = 0;
    inputBufferInfo.range  = compNaiveDistPipeline.inputBufferUBO.size;

    VkWriteDescriptorSet inputDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    inputDescriptorWrite.dstSet           = compNaiveDistPipeline.descriptorSet;
    inputDescriptorWrite.dstBinding       = 0;
    inputDescriptorWrite.dstArrayElement  = 0;
    inputDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputDescriptorWrite.descriptorCount  = 1;
    inputDescriptorWrite.pBufferInfo      = &inputBufferInfo;
    inputDescriptorWrite.pImageInfo       = nullptr;
    inputDescriptorWrite.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &inputDescriptorWrite, 0, nullptr);
  }

  return true;
}

static b8 CreateTriDistComputePipeline()
{
  const Device& device = Renderer::GetDevice();

  // Create descriptor set layout
  {
    VkDescriptorSetLayoutBinding inputBufferLayoutBinding = {};
    inputBufferLayoutBinding.binding            = 0;
    inputBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputBufferLayoutBinding.descriptorCount    = 1;
    inputBufferLayoutBinding.pImmutableSamplers = nullptr;
    inputBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding vertexBufferLayoutBinding = {};
    vertexBufferLayoutBinding.binding            = 1;
    vertexBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBufferLayoutBinding.descriptorCount    = 1;
    vertexBufferLayoutBinding.pImmutableSamplers = nullptr;
    vertexBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding indexBufferLayoutBinding = {};
    indexBufferLayoutBinding.binding            = 2;
    indexBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBufferLayoutBinding.descriptorCount    = 1;
    indexBufferLayoutBinding.pImmutableSamplers = nullptr;
    indexBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelGridLayoutBinding = {};
    voxelGridLayoutBinding.binding            = 3;
    voxelGridLayoutBinding.descriptorCount    = 1;
    voxelGridLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelGridLayoutBinding.pImmutableSamplers = nullptr;
    voxelGridLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding activeCellsBufferLayoutBinding = {};
    activeCellsBufferLayoutBinding.binding            = 4;
    activeCellsBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCellsBufferLayoutBinding.descriptorCount    = 1;
    activeCellsBufferLayoutBinding.pImmutableSamplers = nullptr;
    activeCellsBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        inputBufferLayoutBinding,
        vertexBufferLayoutBinding,
        indexBufferLayoutBinding,
        voxelGridLayoutBinding,
        activeCellsBufferLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &compTriDistPipeline.descriptorSetLayout));
  }

  // Create Descriptor Sets
  {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool     = Renderer::GetDescriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts        = &compTriDistPipeline.descriptorSetLayout;

    VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice, &descriptorSetAllocateInfo, &compTriDistPipeline.descriptorSet));
  }

  // Create pipeline layout
  {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &compTriDistPipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &compTriDistPipeline.pipelineLayout));
  }

  // Create compute pipeline
  {
    // Read compute shader code
    FileSystem::FileContent computeShaderCode;
    if (!FileSystem::ReadAllFileContent("shaders/sdfvoxelgrid/computeTriDist.comp.spv", &computeShaderCode))
    {
      LOG_ERROR("Unable to load compute shader code!");
      return false;
    }

    VkShaderModule computeShaderModule = CreateShaderModule(device, computeShaderCode, nullptr);

    VkPipelineShaderStageCreateInfo computeShaderCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    computeShaderCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderCreateInfo.module = computeShaderModule;
    computeShaderCreateInfo.pName  = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computePipelineCreateInfo.stage  = computeShaderCreateInfo;
    computePipelineCreateInfo.layout = compTriDistPipeline.pipelineLayout;

    VK_SUCCESS_CHECK(vkCreateComputePipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                              &computePipelineCreateInfo, nullptr, &compTriDistPipeline.pipeline));

    vkDestroyShaderModule(device.logicalDevice, computeShaderModule, nullptr);
  }

  // Create UBO voxel grid input buffer
  {
    VkDeviceSize bufferSize = sizeof(UBOCompTriDistInput);
    compTriDistPipeline.inputBufferUBO.Create(device, bufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    // Update descriptor sets
    VkDescriptorBufferInfo inputBufferInfo = {};
    inputBufferInfo.buffer = compTriDistPipeline.inputBufferUBO.buffer;
    inputBufferInfo.offset = 0;
    inputBufferInfo.range  = compTriDistPipeline.inputBufferUBO.size;

    VkWriteDescriptorSet inputDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    inputDescriptorWrite.dstSet           = compTriDistPipeline.descriptorSet;
    inputDescriptorWrite.dstBinding       = 0;
    inputDescriptorWrite.dstArrayElement  = 0;
    inputDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputDescriptorWrite.descriptorCount  = 1;
    inputDescriptorWrite.pBufferInfo      = &inputBufferInfo;
    inputDescriptorWrite.pImageInfo       = nullptr;
    inputDescriptorWrite.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &inputDescriptorWrite, 0, nullptr);
  }

  return true;
}

static b8 CreateJumpFloodingComputePipeline()
{
  const Device& device = Renderer::GetDevice();

  // Create descriptor set layout
  {
    VkDescriptorSetLayoutBinding inputBufferLayoutBinding = {};
    inputBufferLayoutBinding.binding            = 0;
    inputBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputBufferLayoutBinding.descriptorCount    = 1;
    inputBufferLayoutBinding.pImmutableSamplers = nullptr;
    inputBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding activeCellsBufferLayoutBinding = {};
    activeCellsBufferLayoutBinding.binding            = 1;
    activeCellsBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    activeCellsBufferLayoutBinding.descriptorCount    = 1;
    activeCellsBufferLayoutBinding.pImmutableSamplers = nullptr;
    activeCellsBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding updatedCellsBufferLayoutBinding = {};
    updatedCellsBufferLayoutBinding.binding            = 2;
    updatedCellsBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    updatedCellsBufferLayoutBinding.descriptorCount    = 1;
    updatedCellsBufferLayoutBinding.pImmutableSamplers = nullptr;
    updatedCellsBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding voxelGridLayoutBinding = {};
    voxelGridLayoutBinding.binding            = 3;
    voxelGridLayoutBinding.descriptorCount    = 1;
    voxelGridLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    voxelGridLayoutBinding.pImmutableSamplers = nullptr;
    voxelGridLayoutBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        inputBufferLayoutBinding,
        activeCellsBufferLayoutBinding,
        updatedCellsBufferLayoutBinding,
        voxelGridLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &compJumpFloodingPipeline.descriptorSetLayout));
  }

  // Create Descriptor Sets
  {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool     = Renderer::GetDescriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts        = &compJumpFloodingPipeline.descriptorSetLayout;

    VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice, &descriptorSetAllocateInfo, &compJumpFloodingPipeline.descriptorSet));
  }

  // Create pipeline layout
  {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &compJumpFloodingPipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &compJumpFloodingPipeline.pipelineLayout));
  }

  // Create compute pipeline
  {
    // Read compute shader code
    FileSystem::FileContent computeShaderCode;
    if (!FileSystem::ReadAllFileContent("shaders/sdfvoxelgrid/computeJumpFlooding.comp.spv", &computeShaderCode))
    {
      LOG_ERROR("Unable to load compute shader code!");
      return false;
    }

    VkShaderModule computeShaderModule = CreateShaderModule(device, computeShaderCode, nullptr);

    VkPipelineShaderStageCreateInfo computeShaderCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    computeShaderCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderCreateInfo.module = computeShaderModule;
    computeShaderCreateInfo.pName  = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computePipelineCreateInfo.stage  = computeShaderCreateInfo;
    computePipelineCreateInfo.layout = compJumpFloodingPipeline.pipelineLayout;

    VK_SUCCESS_CHECK(vkCreateComputePipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                              &computePipelineCreateInfo, nullptr, &compJumpFloodingPipeline.pipeline));

    vkDestroyShaderModule(device.logicalDevice, computeShaderModule, nullptr);
  }

  // Create UBO voxel grid input buffer
  {
    VkDeviceSize bufferSize = sizeof(UBOCompJumpFloodInput);
    compJumpFloodingPipeline.inputBufferUBO.Create(device, bufferSize,
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    // Update descriptor sets
    VkDescriptorBufferInfo inputBufferInfo = {};
    inputBufferInfo.buffer = compJumpFloodingPipeline.inputBufferUBO.buffer;
    inputBufferInfo.offset = 0;
    inputBufferInfo.range  = compJumpFloodingPipeline.inputBufferUBO.size;

    VkWriteDescriptorSet inputDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    inputDescriptorWrite.dstSet           = compJumpFloodingPipeline.descriptorSet;
    inputDescriptorWrite.dstBinding       = 0;
    inputDescriptorWrite.dstArrayElement  = 0;
    inputDescriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputDescriptorWrite.descriptorCount  = 1;
    inputDescriptorWrite.pBufferInfo      = &inputBufferInfo;
    inputDescriptorWrite.pImageInfo       = nullptr;
    inputDescriptorWrite.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device.logicalDevice, 1, &inputDescriptorWrite, 0, nullptr);
  }

  return true;
}