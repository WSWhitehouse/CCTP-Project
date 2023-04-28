#include "Gizmos.hpp"

// core
#include "core/Logging.hpp"
#include <memory>
#include <queue>

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/AssetDatabase.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// ecs
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"

struct GizmoPushConstant
{
  alignas(16) glm::mat4 WVP;
  alignas(16) glm::vec3 colour;
};

STATIC_ASSERT(sizeof(GizmoPushConstant) % 16 == 0);

static b8 gizmosEnabled = true;

// Wireframe Pipeline
static PipelineHandle wireframePipelineHandle = INVALID_PIPELINE_HANDLE;

// Vertex Buffers
static vk::Buffer wireCubeVertexBuffer = {};

// Gizmo Queues
static std::queue<GizmoPushConstant> wireCube = {};

// Forward Declarations
static void CreateWireframePipeline();
static void RenderWireframePipeline(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
static void CleanUpWireframePipeline();
static void CreateWireCubeVertexBuffer();
static void DestroyWireCubeVertexBuffer();

void Gizmos::Init()
{
  // Pipelines
  CreateWireframePipeline();

  // Vertex Buffers
  CreateWireCubeVertexBuffer();
}

void Gizmos::Destroy()
{
  // Vertex Buffers
  DestroyWireCubeVertexBuffer();
}

void Gizmos::EnableGizmos()  { gizmosEnabled = true;  }
void Gizmos::DisableGizmos() { gizmosEnabled = false; }
void Gizmos::ToggleGizmos()  { gizmosEnabled = !gizmosEnabled; }

void Gizmos::DrawWireCube(glm::vec3 position, glm::vec3 extents, glm::quat rotation, glm::vec3 colour)
{
  const glm::mat4x4 matrix = Math::CreateTRSMatrix(position, rotation, extents * 2.0f);
  const GizmoPushConstant pushConstant =
    {
      .WVP    = matrix,
      .colour = colour
    };

  wireCube.push(pushConstant);
}

static void RenderWireframePipeline(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame)
{
  if (!gizmosEnabled) return;

  const GraphicsPipeline& pipeline = Renderer::GetGraphicsPipeline(wireframePipelineHandle);

  const VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &wireCubeVertexBuffer.buffer, offsets);

  while(!wireCube.empty())
  {
    GizmoPushConstant pushConstant = wireCube.front();
    wireCube.pop();

    // Update matrix with camera matrices
    pushConstant.WVP = camera.projMatrix * camera.viewMatrix * pushConstant.WVP;

    vkCmdPushConstants(cmdBuffer, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(GizmoPushConstant), &pushConstant);

    // Draw the bottom and top faces
    vkCmdDraw(cmdBuffer, 5, 1, 0, 0);
    vkCmdDraw(cmdBuffer, 5, 1, 5, 0);

    // Draw the side faces
    vkCmdDraw(cmdBuffer, 2, 1, 10, 0);
    vkCmdDraw(cmdBuffer, 2, 1, 12, 0);
    vkCmdDraw(cmdBuffer, 2, 1, 14, 0);
    vkCmdDraw(cmdBuffer, 2, 1, 16, 0);
  }
}

static void CreateWireframePipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  // Load shader code from file
  FileSystem::FileContent vertShaderCode, fragShaderCode;

  if (!FileSystem::ReadAllFileContent("shaders/gizmos/wireframe.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
  if (!FileSystem::ReadAllFileContent("shaders/gizmos/wireframe.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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
  VkVertexInputBindingDescription bindingDescription = {};
  bindingDescription.binding   = 0;
  bindingDescription.stride    = sizeof(glm::vec3);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attributeDescriptions = {};
  attributeDescriptions.binding  = 0;
  attributeDescriptions.location = 0;
  attributeDescriptions.format   = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions.offset   = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputInfo.vertexBindingDescriptionCount   = 1;
  vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = 1;
  vertexInputInfo.pVertexAttributeDescriptions    = &attributeDescriptions;

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_LINE;
  rasterizer.lineWidth               = 2.0f;
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
  const VkPushConstantRange pushConstant =
    {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset     = 0,
      .size       = sizeof(GizmoPushConstant)
    };

  const GraphicsPipelineConfig config
    {
      .renderFuncPtr  = RenderWireframePipeline,
      .cleanUpFuncPtr = CleanUpWireframePipeline,

      .renderQueue = OPAQUE_QUEUE,

      .renderPass    = 0,
      .renderSubpass = 0,

      .shaderStageCreateInfoCount = ARRAY_SIZE(shaderStages),
      .shaderStageCreateInfos     = shaderStages,

      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,

      .vertexInputStateCreateInfo   = &vertexInputInfo,
      .rasterizationStateCreateInfo = &rasterizer,
      .multisampleStateCreateInfo   = &multisampling,
      .colourBlendStateCreateInfo   = &colorBlending,
      .depthStencilStateCreateInfo  = &depthStencil,

      .descriptorSetLayoutCount = 0,
      .descriptorSetLayouts     = nullptr,

      .pushConstantRangeCount = 1,
      .pushConstantRanges     = &pushConstant,
    };

  wireframePipelineHandle = Renderer::CreateGraphicsPipeline(config);

  // Destroy shader modules
  vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

  // Clean up file content
  mem_free(vertShaderCode.data);
  mem_free(fragShaderCode.data);
}

static void CleanUpWireframePipeline()
{

}

static void CreateWireCubeVertexBuffer()
{
  const vk::Device& device = Renderer::GetDevice();

  constexpr const glm::vec3 vertices[] =
    {
      // Bottom face
      glm::vec3(-0.5f, -0.5f, -0.5f),
      glm::vec3( 0.5f, -0.5f, -0.5f),
      glm::vec3( 0.5f, -0.5f,  0.5f),
      glm::vec3(-0.5f, -0.5f,  0.5f),
      glm::vec3(-0.5f, -0.5f, -0.5f),

      // Top face
      glm::vec3(-0.5f,  0.5f, -0.5f),
      glm::vec3( 0.5f,  0.5f, -0.5f),
      glm::vec3( 0.5f,  0.5f,  0.5f),
      glm::vec3(-0.5f,  0.5f,  0.5f),
      glm::vec3(-0.5f,  0.5f, -0.5f),

      // Side faces
      glm::vec3(-0.5f, -0.5f, -0.5f),
      glm::vec3(-0.5f,  0.5f, -0.5f),

      glm::vec3( 0.5f, -0.5f, -0.5f),
      glm::vec3( 0.5f,  0.5f, -0.5f),

      glm::vec3( 0.5f, -0.5f,  0.5f),
      glm::vec3( 0.5f,  0.5f,  0.5f),

      glm::vec3(-0.5f, -0.5f,  0.5f),
      glm::vec3(-0.5f,  0.5f,  0.5f),
    };

  VkDeviceSize bufferSize = sizeof(vertices[0]) * ARRAY_SIZE(vertices);

  // Create staging buffer
  vk::Buffer stagingBuffer = {};
  stagingBuffer.Create(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Copy vertex data to staging buffer
  void* data;
  stagingBuffer.MapMemory(device, &data);
  mem_copy(data, vertices, bufferSize);
  stagingBuffer.UnmapMemory(device);

  // Create vertex buffer
  wireCubeVertexBuffer.Create(device, bufferSize,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy staging buffer to vertex buffer
  vk::Buffer::CopyBufferToBuffer(stagingBuffer, wireCubeVertexBuffer, bufferSize);

  // Destroy staging buffer
  stagingBuffer.Destroy(device);
}

static void DestroyWireCubeVertexBuffer()
{
  const vk::Device& device = Renderer::GetDevice();
  wireCubeVertexBuffer.Destroy(device);
}