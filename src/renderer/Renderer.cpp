#include "Renderer.hpp"

// inline
#include "renderer/vk/vkValidation.inl"
#include "renderer/vk/vkExtensions.inl"

// core
#include "application/ApplicationInfo.hpp"
#include "core/Window.hpp"
#include "core/AppTime.hpp"
#include "core/Assert.hpp"
#include "core/Hash.hpp"
#include <atomic>

// containers
#include "containers/FArray.hpp"
#include "containers/DArray.hpp"
#include <unordered_map>

// renderer
#include "renderer/UniformBufferObjects.hpp"
#include "renderer/GraphicsPipeline.hpp"
#include "renderer/Material.hpp"
#include "renderer/Gizmos.hpp"

// geometry
#include "filesystem/AssetDatabase.hpp"
#include "geometry/Mesh.hpp"
#include "geometry/Vertex.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/Camera.hpp"

// ui
#include "ui/UI.hpp"

// vendor
#include "renderer/vendor/ImGuiRenderer.hpp"
#include "stb_image.h"

// --- CORE VK OBJECTS --- //
static VkInstance instance  = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static vk::Device device    = {};

// --- COMMAND POOLS --- //
static vk::CommandPool graphicsCommandPool = {};
static vk::CommandPool computeCommandPool  = {};

// --- RENDER OBJECTS --- //
static vk::Swapchain swapchain         = {};
static VkRenderPass renderPass         = VK_NULL_HANDLE;
static VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

static FArray<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffer = {VK_NULL_HANDLE};

#define MAX_GRAPHICS_PIPELINES 100
static FArray<GraphicsPipeline, MAX_GRAPHICS_PIPELINES> pipelines  = {};
static std::unordered_map<PipelineHandle, u32> pipelineSparseArray = {};
static u32 pipelineCount = 0;

// Core Shader Data (descriptor set 0)
static VkDescriptorSetLayout coreDescriptorSetLayout                   =  VK_NULL_HANDLE;
static FArray<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> coreDescriptorSet = {VK_NULL_HANDLE};

static FArray<vk::Buffer,     MAX_FRAMES_IN_FLIGHT> frameDataUBO        = {{}};
static FArray<UBOFrameData*,  MAX_FRAMES_IN_FLIGHT> frameDataUBOMapped  = {nullptr};
static FArray<vk::Buffer,     MAX_FRAMES_IN_FLIGHT> cameraDataUBO       = {{}};
static FArray<UBOCameraData*, MAX_FRAMES_IN_FLIGHT> cameraDataUBOMapped = {nullptr};

// --- SYNC OBJECTS --- //
static u32 currentFrame = 0;
static FArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphore = {VK_NULL_HANDLE};
static FArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphore = {VK_NULL_HANDLE};
static FArray<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFence           = {VK_NULL_HANDLE};

static std::atomic<u64> frameCount = 0;

extern vk::Image uiImage;
extern VkImageView uiImageView;
extern VkSemaphore uiRenderFinishedSemaphore;

// TODO(WSWhitehouse): Remove these...
// Primitive Meshes
MeshBufferData quadMesh = {};

// --- FORWARD DECLARATIONS --- //
// Instance
static b8 CreateInstance();

// Device
static b8 PickPhysicalDevice();
static vk::QueueFamilyIndices FindQueueFamilyIndices(const VkPhysicalDevice physicalDevice);
static b8 IsPhysicalDeviceSuitable(const VkPhysicalDevice physicalDevice);
static b8 CheckPhysicalDeviceExtensionSupport(const VkPhysicalDevice physicalDevice);
static vk::SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice physicalDevice);
static VkSampleCountFlagBits GetMaxSampleCount(const VkPhysicalDevice physicalDevice);
static b8 CreateLogicalDevice();

// Swapchain
static void CreateSwapChain();
static void CreateSwapChainImages();
static b8 CreateDepthBuffer();
static void RecreateSwapChain();
static void CreateFramebuffers();

// Render Pipeline
static b8 CreateRenderPass();
static b8 CreateCommandPools();
static b8 CreateCommandBuffer();
static void CreateDescriptorPool();

static void DestroyGraphicsPipeline(GraphicsPipeline pipeline);

static void CreateCoreDescriptorsAndBuffers();
static void DestroyCoreDescriptorsAndBuffers();

static void RecordRenderData(ECS::Manager& ecs, VkCommandBuffer cmdBuffer);

extern vk::Buffer CreateVertexBuffer(const Vertex* vertexArray, u64 vertexCount);
extern vk::Buffer CreateIndexBuffer(const void* indexArray, u64 indexCount, u64 sizeOfIndex);

// Shutdown
static void DestroySwapChain();

// --- GETTERS --- //
[[nodiscard]]       VkInstance       Renderer::GetInstance() noexcept            { return instance; }
[[nodiscard]] const vk::Device&      Renderer::GetDevice() noexcept              { return device; }
[[nodiscard]] const vk::Swapchain&   Renderer::GetSwapchain() noexcept           { return swapchain; }
[[nodiscard]]       VkRenderPass     Renderer::GetRenderpass() noexcept          { return renderPass; }
[[nodiscard]]       VkDescriptorPool Renderer::GetDescriptorPool() noexcept      { return descriptorPool; }
[[nodiscard]] const vk::CommandPool& Renderer::GetGraphicsCommandPool() noexcept { return graphicsCommandPool; }
[[nodiscard]] const vk::CommandPool& Renderer::GetComputeCommandPool() noexcept  { return computeCommandPool; }

static b8 windowResize = false;
static void WindowResizeCallback(i32 width, i32 height)
{
  windowResize = true;
}

b8 Renderer::Init()
{
  LOG_INFO("Renderer Initialising...");

  Window::SetOnWindowResizedCallback(WindowResizeCallback);

  // Instance & Surface
  if (!CreateInstance()) return false;
  if (!vk::CreateVkSurface(instance, nullptr, &surface)) return false;

  // Device
  if (!PickPhysicalDevice()) return false;
  if (!CreateLogicalDevice()) return false;

  // Swapchain
  CreateSwapChain();
  CreateSwapChainImages();
  if (!CreateDepthBuffer()) return false;

  // Sync Objects
  {
    VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
      VK_SUCCESS_CHECK(vkCreateSemaphore(device.logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore[i]));
      VK_SUCCESS_CHECK(vkCreateSemaphore(device.logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore[i]));
      VK_SUCCESS_CHECK(vkCreateFence(device.logicalDevice, &fenceInfo, nullptr, &inFlightFence[i]));
    }
  }

  if (!CreateCommandPools()) return false;
  if (!CreateCommandBuffer()) return false;
  CreateDescriptorPool();

  if (!CreateRenderPass()) return false;
  UI::Init();

  CreateFramebuffers();
  CreateCoreDescriptorsAndBuffers();
  Material::InitMaterialSystem();

  Gizmos::Init();
  ImGuiRenderer::Init();

  // Create buffers and initialise renderer data...
  {
    const Mesh* quad             = Mesh::CreatePrimitive(Primitive::QUAD);
    const MeshNode& node         = quad->nodeArray[0];
    const MeshGeometry& geometry = quad->geometryArray[node.geometryIndex];

    switch (geometry.indexType)
    {
      case IndexType::U16_TYPE:
      {
        quadMesh.indexType = VK_INDEX_TYPE_UINT16;
        break;
      }
      case IndexType::U32_TYPE:
      {
        quadMesh.indexType = VK_INDEX_TYPE_UINT32;
        break;
      }

      default:
      {
        LOG_FATAL("Unhandled Mesh Geometry Index Type Case!");
        break;
      }
    }

    quadMesh.vertexBuffer = CreateVertexBuffer(geometry.vertexArray, geometry.vertexCount);
    quadMesh.indexBuffer  = CreateIndexBuffer(geometry.indexArray, geometry.indexCount, geometry.SizeOfIndex());
    quadMesh.indexCount   = geometry.indexCount;
  }

  LOG_INFO("Renderer Initialisation Complete!");
  return true;
}

void Renderer::Shutdown()
{
  LOG_INFO("Renderer Shutting Down...");
  Renderer::WaitForDeviceIdle();

  // TODO(WSWhitehouse): Properly wait for all resources to be destroyed, this is only notifying them...
  frameCount.fetch_add(MAX_FRAMES_IN_FLIGHT + 1);
  frameCount.notify_all();

  ImGuiRenderer::Shutdown();
  Gizmos::Destroy();

  UI::Shutdown();

  // Destroy quad primitive
  quadMesh.vertexBuffer.Destroy(device);
  quadMesh.indexBuffer.Destroy(device);

  for (u32 i = 0; i < pipelineCount; ++i)
  {
    DestroyGraphicsPipeline(pipelines[i]);
  }

  Material::ShutdownMaterialSystem();
  DestroyCoreDescriptorsAndBuffers();

  vkDestroyDescriptorPool(device.logicalDevice, descriptorPool, nullptr);

  LOG_INFO("Destroying Sync Objects!");
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(device.logicalDevice, imageAvailableSemaphore[i], nullptr);
    vkDestroySemaphore(device.logicalDevice, renderFinishedSemaphore[i], nullptr);
    vkDestroyFence    (device.logicalDevice, inFlightFence[i],           nullptr);
  }

  LOG_INFO("Destroying Command Pool!");
  graphicsCommandPool.Destroy(GetDevice());
  computeCommandPool.Destroy(GetDevice());

  DestroySwapChain();

  // Destroy render pass
  LOG_INFO("Destroying Render Pass!");
  vkDestroyRenderPass(device.logicalDevice, renderPass, nullptr);

  // Destroy Surface
  LOG_INFO("Destroying Surface!");
  vkDestroySurfaceKHR(instance, surface, nullptr);

  // Destroy Logical Device
  LOG_INFO("Destroying Logical Device!");
  vkDestroyDevice(device.logicalDevice, nullptr);

#if VK_VALIDATION
  // Destroy vk validation callback if it exists and VK_VALIDATION is enabled
  if (vkValidationMessengerHandle != VK_NULL_HANDLE)
  {
    LOG_INFO("Destroying Vulkan Validation Messenger!");
    // NOTE(WSWhitehouse): Have to find function address as this is an extension not part of vk core
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger_func =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    destroy_messenger_func(instance, vkValidationMessengerHandle, nullptr);
  }
#endif

  // Destroy Instance
  LOG_INFO("Destroying Vulkan Instance!");
  vkDestroyInstance(instance, nullptr);

  LOG_INFO("Renderer Shutdown Complete!");
}

void Renderer::BeginFrame(ECS::Manager& ecs)
{
  ImGuiRenderer::NewFrame();

  UI::DrawUI(ecs);
}

void Renderer::DrawFrame(ECS::Manager& ecs)
{
  vkWaitForFences(device.logicalDevice, 1, &inFlightFence[currentFrame], VK_TRUE, U64_MAX);
  frameCount.fetch_add(1, std::memory_order_seq_cst);
  frameCount.notify_all();

  if (windowResize)
  {
    RecreateSwapChain();
    return;
  }

  u32 imageIndex;
  VkResult nextImageResult = vkAcquireNextImageKHR(
          device.logicalDevice, swapchain.swapchain, U64_MAX,
          imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);

  switch (nextImageResult)
  {
    // NOTE(WSWhitehouse): These result codes result in a recreation of the swapchain:
    case VK_ERROR_OUT_OF_DATE_KHR:
    {
      RecreateSwapChain();
      return;
    }

    // NOTE(WSWhitehouse): These result codes are treated as success:
    case VK_SUBOPTIMAL_KHR:
    case VK_SUCCESS:
    {
      break;
    }

    // NOTE(WSWhitehouse): Any other result we go through the VK_SUCCESS_CHECK
    default:
    {
      VK_SUCCESS_CHECK(nextImageResult);
      break;
    }
  }

  vkResetFences(device.logicalDevice, 1, &inFlightFence[currentFrame]);
  vkResetCommandBuffer(commandBuffer[currentFrame], 0);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags            = 0;
  beginInfo.pInheritanceInfo = nullptr;

  VK_SUCCESS_CHECK(vkBeginCommandBuffer(commandBuffer[currentFrame], &beginInfo));

  // NOTE(WSWhitehouse): MSVC complains about inlining the offset variable when setting
  // up the render pass begin info and the scissor...
  VkOffset2D offset = {0,0};

//  constexpr const VkClearValue colourClearVal = { .color        = {{0.64f, 0.17f, 0.77f, 1.0f}} };
  constexpr const VkClearValue colourClearVal = { .color        = {{0.05f, 0.05f, 0.05f, 1.0f}} };
  constexpr const VkClearValue depthClearVal  = { .depthStencil = {1.0f, 0} };
  constexpr const VkClearValue clearValues[]  =
    {
      colourClearVal,
      depthClearVal
    };

  VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass        = renderPass;
  renderPassInfo.framebuffer       = swapchain.framebuffers[imageIndex];
  renderPassInfo.renderArea.offset = offset;
  renderPassInfo.renderArea.extent = swapchain.extent;
  renderPassInfo.clearValueCount   = ARRAY_SIZE(clearValues);
  renderPassInfo.pClearValues      = clearValues;

  vkCmdBeginRenderPass(commandBuffer[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {};
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = (f32)swapchain.extent.width;
  viewport.height   = (f32)swapchain.extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer[currentFrame], 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset   = offset;
  scissor.extent   = swapchain.extent;
  vkCmdSetScissor(commandBuffer[currentFrame], 0, 1, &scissor);

  RecordRenderData(ecs, commandBuffer[currentFrame]);
  ImGuiRenderer::Render(commandBuffer[currentFrame]);

  vkCmdNextSubpass(commandBuffer[currentFrame], VK_SUBPASS_CONTENTS_INLINE);

  UI::BlitUI(commandBuffer[currentFrame]);

  vkCmdEndRenderPass(commandBuffer[currentFrame]);

  VK_SUCCESS_CHECK(vkEndCommandBuffer(commandBuffer[currentFrame]));

  const VkSemaphore waitSemaphores[]   = {imageAvailableSemaphore[currentFrame], uiRenderFinishedSemaphore};
  const VkSemaphore signalSemaphores[] = {renderFinishedSemaphore[currentFrame]};

  const u32 waitSemaphoresCount = UI::HasRedrawnThisFrame() ?
                                    ARRAY_SIZE(waitSemaphores) :
                                    ARRAY_SIZE(waitSemaphores) - 1;

  const VkPipelineStageFlags waitStages[] =
    {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount   = waitSemaphoresCount;
  submitInfo.pWaitSemaphores      = waitSemaphores;
  submitInfo.pWaitDstStageMask    = waitStages;
  submitInfo.signalSemaphoreCount = ARRAY_SIZE(signalSemaphores);
  submitInfo.pSignalSemaphores    = signalSemaphores;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &commandBuffer[currentFrame];

  VK_SUCCESS_CHECK(vkQueueSubmit(device.graphicsQueue, 1, &submitInfo, inFlightFence[currentFrame]));

  VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = signalSemaphores;
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &swapchain.swapchain;
  presentInfo.pImageIndices      = &imageIndex;
  presentInfo.pResults           = nullptr;

  const VkResult queuePresentResult = vkQueuePresentKHR(device.presentQueue, &presentInfo);

  UI::ResetHasRedrawnThisFrame();

  switch (queuePresentResult)
  {
    // NOTE(WSWhitehouse): These result codes result in a recreation of the swapchain:
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    {
      RecreateSwapChain();
      return;
    }

    // NOTE(WSWhitehouse): These result codes are treated as success:
    case VK_SUCCESS:
    {
      break;
    }

    // NOTE(WSWhitehouse): Any other result we go through the VK_SUCCESS_CHECK
    default:
    {
      VK_SUCCESS_CHECK(nextImageResult);
      break;
    }
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::EndFrame()
{

}

void Renderer::WaitForDeviceIdle()
{
  vkDeviceWaitIdle(device.logicalDevice);
}

u64 Renderer::GetFrameNumber()
{
  return frameCount.load(std::memory_order::acquire);
}

void Renderer::WaitForFrame(u32 frame)
{
  while(true)
  {
    const u32 frameNum = GetFrameNumber();
    if (frame == frameNum) return;

    frameCount.wait(frameNum);
  }
}

#define PIPELINE_HANDLE_FUNC_HASH_BITS 32ULL
#define PIPELINE_HANDLE_FUNC_HASH_MASK ((1ULL << PIPELINE_HANDLE_FUNC_HASH_BITS) - 1ULL)
#define PIPELINE_HANDLE_GET_FUNC_HASH(handle) ((handle) & PIPELINE_HANDLE_FUNC_HASH_MASK)

#define PIPELINE_HANDLE_RENDER_PASS_BITS 8ULL
#define PIPELINE_HANDLE_RENDER_PASS_MASK ((1ULL << PIPELINE_HANDLE_RENDER_PASS_BITS) - 1ULL)
#define PIPELINE_HANDLE_GET_RENDER_PASS(handle) (((handle) >> (PIPELINE_HANDLE_FUNC_HASH_BITS \
                                                               )) & PIPELINE_HANDLE_RENDER_PASS_MASK)

#define PIPELINE_HANDLE_RENDER_SUBPASS_BITS 8ULL
#define PIPELINE_HANDLE_RENDER_SUBPASS_MASK ((1ULL << PIPELINE_HANDLE_RENDER_SUBPASS_BITS) - 1ULL)
#define PIPELINE_HANDLE_GET_RENDER_SUBPASS(handle) (((handle) >> (PIPELINE_HANDLE_FUNC_HASH_BITS + \
                                                                  PIPELINE_HANDLE_RENDER_PASS_BITS \
                                                                  )) & PIPELINE_HANDLE_RENDER_SUBPASS_MASK)

#define PIPELINE_HANDLE_QUEUE_BITS 8ULL
#define PIPELINE_HANDLE_QUEUE_MASK ((1ULL << PIPELINE_HANDLE_QUEUE_BITS) - 1ULL)
#define PIPELINE_HANDLE_GET_QUEUE(handle) (((handle) >> (PIPELINE_HANDLE_FUNC_HASH_BITS   +  \
                                                         PIPELINE_HANDLE_RENDER_PASS_BITS +  \
                                                         PIPELINE_HANDLE_RENDER_SUBPASS_BITS \
                                                         )) & PIPELINE_HANDLE_QUEUE_MASK)

static i32 QSortGraphicsPipelines(const void* lhs, const void* rhs)
{
  const PipelineHandle* handleLhs = (PipelineHandle*)lhs;
  const PipelineHandle* handleRhs = (PipelineHandle*)rhs;

  const u32 renderPassLhs = (u32)PIPELINE_HANDLE_GET_RENDER_PASS(*handleLhs);
  const u32 renderPassRhs = (u32)PIPELINE_HANDLE_GET_RENDER_PASS(*handleRhs);

  if (renderPassLhs < renderPassRhs) return -1;
  if (renderPassLhs > renderPassRhs) return  1;

  const u32 renderSubpassLhs = (u32)PIPELINE_HANDLE_GET_RENDER_SUBPASS(*handleLhs);
  const u32 renderSubpassRhs = (u32)PIPELINE_HANDLE_GET_RENDER_SUBPASS(*handleRhs);

  if (renderSubpassLhs < renderSubpassRhs) return -1;
  if (renderSubpassLhs > renderSubpassRhs) return  1;

  const u32 queueLhs = (u32)PIPELINE_HANDLE_GET_QUEUE((*handleLhs));
  const u32 queueRhs = (u32)PIPELINE_HANDLE_GET_QUEUE((*handleRhs));

  if (queueLhs < queueRhs) return -1;
  if (queueLhs > queueRhs) return  1;

  return 0;
}

static INLINE void SortGraphicsPipelines()
{
  if (pipelineCount == 0) return;

  ::qsort(pipelines.data, pipelineCount, sizeof(GraphicsPipeline), QSortGraphicsPipelines);

  // NOTE(WSWhitehouse): After sorting ensure the sparse array values point to the correct index...
  for (u32 i = 0; i < pipelineCount; ++i)
  {
    const PipelineHandle& handle = pipelines[i].handle;
    pipelineSparseArray[handle]  = i;
  }
}

PipelineHandle Renderer::CreateGraphicsPipeline(const GraphicsPipelineConfig& config)
{
  ASSERT_MSG(pipelineCount < MAX_GRAPHICS_PIPELINES, "Max pipeline count hit! Please consider increasing the max pipeline count!");

  // NOTE(WSWhitehouse): Ensure the config is set up correctly...
  ASSERT_MSG(config.shaderStageCreateInfoCount   >= 1,       "A graphics pipeline requires at least 1 shader!");
  ASSERT_MSG(config.renderFuncPtr                != nullptr, "Render Function Pointer is nullptr!");
  ASSERT_MSG(config.shaderStageCreateInfos       != nullptr, "Shader Stage Create Info is nullptr!");
  ASSERT_MSG(config.vertexInputStateCreateInfo   != nullptr, "Vertex Input Stage Create Info is nullptr!");
  ASSERT_MSG(config.rasterizationStateCreateInfo != nullptr, "Rasterization Stage Create Info is nullptr!");
  ASSERT_MSG(config.multisampleStateCreateInfo   != nullptr, "Multisample Stage Create Info is nullptr!");
  ASSERT_MSG(config.colourBlendStateCreateInfo   != nullptr, "Colour Blend Stage Create Info is nullptr!");
  ASSERT_MSG(config.depthStencilStateCreateInfo  != nullptr, "Depth Stencil Stage Create Info is nullptr!");

  const u32 hashedFuncPtr = Hash::FNV1a32((void*)config.renderFuncPtr, sizeof(GraphicsPipeline::RenderFuncPtr));

  PipelineHandle handle = (PipelineHandle)0;
  handle |= (u64)(hashedFuncPtr);
  handle |= (u64)(config.renderPass)    <<  PIPELINE_HANDLE_FUNC_HASH_BITS;
  handle |= (u64)(config.renderSubpass) << (PIPELINE_HANDLE_FUNC_HASH_BITS + PIPELINE_HANDLE_RENDER_PASS_BITS);
  handle |= (u64)(config.renderQueue)   << (PIPELINE_HANDLE_FUNC_HASH_BITS + PIPELINE_HANDLE_RENDER_PASS_BITS + PIPELINE_HANDLE_RENDER_SUBPASS_BITS);

  if (pipelineSparseArray.contains(handle))
  {
    LOG_ERROR("Trying to register a duplicate pipeline!");
    return INVALID_PIPELINE_HANDLE;
  }

  GraphicsPipeline newPipeline = {};
  newPipeline.handle         = handle;
  newPipeline.renderFuncPtr  = config.renderFuncPtr;
  newPipeline.cleanUpFuncPtr = config.cleanUpFuncPtr;

  // Dynamic State
  constexpr const VkDynamicState dynamicStates[]
    {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };

  VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicState.dynamicStateCount = ARRAY_SIZE(dynamicStates);
  dynamicState.pDynamicStates    = dynamicStates;

  // Input Assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssembly.topology               = config.primitiveTopology;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Viewport & Scissor
  // NOTE(WSWhitehouse): We defined the viewport and scissor as a dynamic state above, so no need to define them here.
  // It will be done at draw time instead. https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
  VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewportState.viewportCount = 1;
  viewportState.scissorCount  = 1;

  DArray<VkDescriptorSetLayout> descriptorSetLayouts = {};
  descriptorSetLayouts.Create(config.descriptorSetLayoutCount + 1);
  descriptorSetLayouts.Add(coreDescriptorSetLayout);

  for (u32 i = 0; i < config.descriptorSetLayoutCount; ++i)
  {
    descriptorSetLayouts.Add(config.descriptorSetLayouts[i]);
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount         = descriptorSetLayouts.Size();
  pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data;
  pipelineLayoutInfo.pushConstantRangeCount = config.pushConstantRangeCount;
  pipelineLayoutInfo.pPushConstantRanges    = config.pushConstantRanges;

  VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &newPipeline.layout));

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipelineCreateInfo.stageCount          = config.shaderStageCreateInfoCount;
  pipelineCreateInfo.pStages             = config.shaderStageCreateInfos;
  pipelineCreateInfo.pVertexInputState   = config.vertexInputStateCreateInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  pipelineCreateInfo.pViewportState      = &viewportState;
  pipelineCreateInfo.pRasterizationState = config.rasterizationStateCreateInfo;
  pipelineCreateInfo.pMultisampleState   = config.multisampleStateCreateInfo;
  pipelineCreateInfo.pDepthStencilState  = config.depthStencilStateCreateInfo;
  pipelineCreateInfo.pColorBlendState    = config.colourBlendStateCreateInfo;
  pipelineCreateInfo.pDynamicState       = &dynamicState;
  pipelineCreateInfo.layout              = newPipeline.layout;
  pipelineCreateInfo.renderPass          = renderPass; // TODO(WSWhitehouse): Support config renderpass.
  pipelineCreateInfo.subpass             = config.renderSubpass;

  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineCreateInfo.basePipelineIndex  = -1;

  VK_SUCCESS_CHECK(vkCreateGraphicsPipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                             &pipelineCreateInfo, nullptr, &newPipeline.pipeline));

  descriptorSetLayouts.Destroy();

  pipelines[pipelineCount] = newPipeline;
  pipelineSparseArray.insert(std::make_pair(handle, pipelineCount));
  pipelineCount++;

  SortGraphicsPipelines();
  return handle;
}

void Renderer::DestroyGraphicsPipeline(PipelineHandle pipelineHandle)
{
  if (!pipelineSparseArray.contains(pipelineHandle))
  {
    LOG_ERROR("Trying to destroy a graphics pipeline that does not exist!");
    return;
  }

  pipelineCount--;

  const u32 lastPipelineIndex = pipelineCount;
  const u32 pipelineIndex     = pipelineSparseArray[pipelineHandle];

  // Destroy the graphics pipeline
  {
    DestroyGraphicsPipeline(pipelines[pipelineIndex]);
    pipelineSparseArray.erase(pipelineHandle);
  }

  // Move the last pipeline into the missing gap
  {
    mem_copy(&pipelines[pipelineIndex], &pipelines[lastPipelineIndex], sizeof(GraphicsPipeline));
    pipelineSparseArray[pipelines[pipelineIndex].handle] = pipelineIndex;
  }

  SortGraphicsPipelines();
}

const GraphicsPipeline& Renderer::GetGraphicsPipeline(PipelineHandle pipelineHandle)
{
  const u32 pipelineIndex = pipelineSparseArray[pipelineHandle];
  return pipelines[pipelineIndex];
}

static void DestroyGraphicsPipeline(GraphicsPipeline pipeline)
{
  if (pipeline.cleanUpFuncPtr != nullptr) { pipeline.cleanUpFuncPtr(); }

  vkDestroyPipeline(device.logicalDevice, pipeline.pipeline, nullptr);
  vkDestroyPipelineLayout(device.logicalDevice, pipeline.layout, nullptr);
}

static void RecordRenderData(ECS::Manager& ecs, VkCommandBuffer cmdBuffer)
{
  using namespace ECS;

  // Update frame data
  UBOFrameData* frameData = frameDataUBOMapped[currentFrame];
  mem_zero(frameData, sizeof(UBOFrameData));

  ComponentSparseSet* lightSparseSet = ecs.GetComponentSparseSet<PointLight>();
  for (u32 i = 0; i < lightSparseSet->componentCount; ++i)
  {
    const ComponentData<PointLight>& data = ((ComponentData<PointLight>*)lightSparseSet->componentArray)[i];

    glm::vec3 position = {};
    if (ecs.HasComponent<Transform>(data.entity))
    {
      Transform* transform = ecs.GetComponent<Transform>(data.entity);
      position = transform->position;
    }

    frameData->pointLights[i].position = position;
    frameData->pointLights[i].colour   = data.component.colour;
    frameData->pointLights[i].range    = data.component.range;
  }

  frameData->pointLightCount = (i32)lightSparseSet->componentCount;
  frameData->ambientColour   = { 0.25f, 0.25f, 0.25f };
  frameData->time            = (f32)AppTime::AppTotalTime();
  frameData->sinTime         = glm::sin(frameData->time);

  ComponentSparseSet* cameraSparseSet = ecs.GetComponentSparseSet<Camera>();
  for (u32 camIndex = 0; camIndex < cameraSparseSet->componentCount; ++camIndex)
  {
    const ComponentData<Camera>& camComponentData = ((ComponentData<Camera>*)cameraSparseSet->componentArray)[camIndex];
    const Camera& camera = camComponentData.component;

    if (!ecs.HasComponent<Transform>(camComponentData.entity)) continue;

    Transform* cameraTransform = ecs.GetComponent<Transform>(camComponentData.entity);

    // Update camera data
    UBOCameraData* cameraData = cameraDataUBOMapped[currentFrame];
    mem_zero(cameraData, sizeof(UBOCameraData));

    cameraData->position   = cameraTransform->position;
    cameraData->viewMat    = camera.viewMatrix;
    cameraData->projMat    = camera.projMatrix;
    cameraData->invViewMat = camera.inverseViewMatrix;
    cameraData->invProjMat = camera.inverseProjMatrix;

    for (u32 pipelineIndex = 0; pipelineIndex < pipelineCount; ++pipelineIndex)
    {
      const GraphicsPipeline& pipeline = pipelines[pipelineIndex];
      vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
      vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                              0, 1, &coreDescriptorSet[currentFrame], 0, nullptr);

      pipeline.renderFuncPtr(ecs, camera, cmdBuffer, currentFrame);
    }
  }

//    // Render all sdf voxel grids...
//    ComponentSparseSet* voxelGridSparseSet = ecs.GetComponentSparseSet<SdfVoxelGrid>();
//    for (u32 voxelGridIndex = 0; voxelGridIndex < voxelGridSparseSet->componentCount; ++voxelGridIndex)
//    {
//      ComponentData<SdfVoxelGrid>& componentData = ((ComponentData<SdfVoxelGrid>*)voxelGridSparseSet->componentArray)[voxelGridIndex];
//      SdfVoxelGrid& voxelGrid = componentData.component;
//
//      Transform* transform = ecs.GetComponent<Transform>(componentData.entity);
//
//      ImGui::Begin("SDF Voxel Grid");
//      ImGui::InputFloat3("Position", glm::value_ptr(transform->position));
//      ImGui::InputFloat3("Rotation", glm::value_ptr(transform->rotation));
//      ImGui::InputFloat3("Scale", glm::value_ptr(transform->scale));
//      ImGui::InputFloat3("Twist", glm::value_ptr(voxelGrid.twist));
//      ImGui::Checkbox("Show Bounds", &voxelGrid.showBounds);
//      ImGui::End();
//
//      UBOSdfVoxelData data = {};
//      data.WVP            = transform->GetWVPMatrix(camera);
//      data.worldMat       = transform->matrix;
//      data.invWorldMat    = glm::inverse(transform->matrix);
//      data.cellCount      = voxelGrid.cellCount;
//      data.twist          = voxelGrid.twist;
//      data.voxelGridScale = voxelGrid.scalingFactor;
//      data.showBounds     = voxelGrid.showBounds;
//      mem_copy(voxelGrid.dataUniformBuffersMapped[currentFrame], &data, sizeof(data));
//
//      GraphicsPipelines::BindPipeline(cmdBuffer, GraphicsPipelines::SDF_VOXEL, currentFrame, frameData, cameraData);
//      GraphicsPipelines::BindDescriptorSet(cmdBuffer, GraphicsPipelines::SDF_VOXEL, &voxelGrid.descriptorSets[currentFrame], 1);
//      vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // Fullscreen Quad
//    }
}

static void DestroySwapChain()
{
  // Destroy swapchain image views, depth images and framebuffers
  LOG_INFO("Destroying Swap Chain Image Views, Depth Images & Framebuffers!");
  for (u32 i = 0; i < swapchain.imagesCount; i++)
  {
    vkDestroyFramebuffer(device.logicalDevice, swapchain.framebuffers[i], nullptr);
    vkDestroyImageView(device.logicalDevice, swapchain.imageViews[i], nullptr);
    vkDestroyImageView(device.logicalDevice, swapchain.depthImageViews[i], nullptr);
    swapchain.depthImages[i].Destroy(device);
  }

  // Destroy swapchain
  LOG_INFO("Destroying Swap Chain!");
  vkDestroySwapchainKHR(device.logicalDevice, swapchain.swapchain, nullptr);

  // NOTE(WSWhitehouse): Ensure we free all the memory associated with the swapchain
  mem_free(swapchain.images);
  mem_free(swapchain.imageViews);
  mem_free(swapchain.depthImages);
  mem_free(swapchain.depthImageViews);
  mem_free(swapchain.framebuffers);
}

b8 CreateInstance()
{
  LOG_INFO("Creating Vulkan Instance...");

  const u32 appVer = VK_MAKE_VERSION(Application::VersionMajor, Application::VersionMinor, Application::VersionPatch);

  // Set up app info
  VkApplicationInfo appInfo  = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  appInfo.apiVersion         = VK_API_VERSION_1_2;
  appInfo.pApplicationName   = Application::Name;
  appInfo.applicationVersion = appVer;
  appInfo.pEngineName        = "Snowflake";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);

  VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  createInfo.pApplicationInfo        = &appInfo;
  createInfo.enabledExtensionCount   = ARRAY_SIZE(instanceExtensions);
  createInfo.ppEnabledExtensionNames = instanceExtensions;

#if defined(VK_VALIDATION)
  createInfo.enabledLayerCount       = ARRAY_SIZE(validationLayers);
  createInfo.ppEnabledLayerNames     = validationLayers;
#else
  createInfo.enabledLayerCount       = 0;
  createInfo.ppEnabledLayerNames     = nullptr;
#endif

  // NOTE(WSWhitehouse): Check for validation layers and print enabled extensions...
#if defined(VK_VALIDATION)
  LOG_INFO("Required Vulkan Extensions:");
  for (u32 i = 0; i < ARRAY_SIZE(instanceExtensions); i++)
  {
    LOG_INFO("\t%s", instanceExtensions[i]);
  }

  LOG_INFO("Vulkan Validation Layers Enabled");

  // Get all available layers
  u32 availableLayerCount = 0;
  VK_SUCCESS_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));
  std::vector<VkLayerProperties> availableLayers(availableLayerCount);
  VK_SUCCESS_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data()));

  // Check required layers are available
  LOG_INFO("Checking if required layers are available:");
  for (u32 i = 0; i < ARRAY_SIZE(validationLayers); i++)
  {
    b8 found = false;
    for (u32 j = 0; j < availableLayerCount; j++)
    {
      if (str_equal(validationLayers[i], availableLayers[j].layerName))
      {
        found = true;
        LOG_INFO("\t%s : found", validationLayers[i]);
        break;
      }
    }

    if (!found)
    {
      LOG_INFO("\t%s : not found", validationLayers[i]);
      LOG_FATAL("Renderer Initialisation Failed! Required validation layer is not available.");
      return false;
    }
  }

  LOG_INFO("All required validation layers found!");
#endif

  VK_SUCCESS_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
  LOG_INFO("Created Instance!");

  // Create Vulkan Validation Messenger Callback
#if defined(VK_VALIDATION)
  LOG_INFO("Creating Vulkan Validation Messenger...");

  VkDebugUtilsMessageSeverityFlagsEXT severity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;//    |
  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

  VkDebugUtilsMessageTypeFlagsEXT type =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

  VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debugMessengerCreateInfo.messageSeverity = severity;
  debugMessengerCreateInfo.messageType     = type;
  debugMessengerCreateInfo.pfnUserCallback = vkValidationMessenger;

  // NOTE(WSWhitehouse): Have to find function address as this is an extension not part of vk core
  PFN_vkCreateDebugUtilsMessengerEXT createMessengerFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  ASSERT_MSG(createMessengerFunc != nullptr, "Failed to get proc address for debug utils messenger! Creating Vulkan Validation Callback failed.")

  VK_SUCCESS_CHECK(createMessengerFunc(instance, &debugMessengerCreateInfo, nullptr, &vkValidationMessengerHandle));
  LOG_INFO("Vulkan Validation Messenger Created!");
#endif

  return true;
}

b8 PickPhysicalDevice()
{
  LOG_INFO("Picking Physical Device...");

  u32 deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  if (deviceCount == 0)
  {
    LOG_FATAL("Failed to find a physical device with Vulkan support!");
    return false;
  }

  VkPhysicalDevice devices[32];
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

  VkPhysicalDevice chosenDevice = VK_NULL_HANDLE;
  for (u32 i = 0; i < deviceCount; i++)
  {
    if (!IsPhysicalDeviceSuitable(devices[i])) continue;

    // This device is suitable - choose it!
    chosenDevice = devices[i];
    break;
  }

  if (chosenDevice == VK_NULL_HANDLE)
  {
    LOG_FATAL("Failed to find a suitable physical device!");
    return false;
  }

  // Set the physical device and get its info
  device.physicalDevice     = chosenDevice;
  device.msaaSampleCount    = GetMaxSampleCount(device.physicalDevice);
  device.queueFamilyIndices = FindQueueFamilyIndices(device.physicalDevice);
  vkGetPhysicalDeviceProperties(device.physicalDevice, &device.properties);
  vkGetPhysicalDeviceFeatures(device.physicalDevice, &device.features);
  vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &device.memory);

  LOG_INFO("Physical Device Selected! (%s)", device.properties.deviceName);
  return true;
}

static vk::QueueFamilyIndices FindQueueFamilyIndices(const VkPhysicalDevice physicalDevice)
{
  vk::QueueFamilyIndices indices = {};

  u32 queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

  VkQueueFamilyProperties queueFamilies[32];
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

  for (u32 i = 0; i < queueFamilyCount; i++)
  {
    // Check for graphics family
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphicsFamily = i;
      indices.graphicsFound  = true;
    }

    // Check for compute family
    if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
      indices.computeFamily = i;
      indices.computeFound  = true;
    }

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &present_support);

    if (present_support)
    {
      indices.presentFamily = i;
      indices.presentFound  = true;
    }
  }

  return indices;
}

static b8 IsPhysicalDeviceSuitable(const VkPhysicalDevice physicalDevice)
{
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(physicalDevice, &features);

  // NOTE(WSWhitehouse): Ensure the device supports the following features:
  if (features.samplerAnisotropy == VK_FALSE) return false;

  VkPhysicalDeviceMemoryProperties memory;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);

  // Check for a discrete GPU device
//  if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return false;

  // Check for queue family support
  vk::QueueFamilyIndices indices = FindQueueFamilyIndices(physicalDevice);
  if (!indices.graphicsFound) return false;
  if (!indices.presentFound)  return false;
  if (!indices.computeFound)  return false;

  // Check for device extension
  if (!CheckPhysicalDeviceExtensionSupport(physicalDevice)) return false;

  // Check for swapchain support
  vk::SwapChainSupportDetails swapchainSupport = QuerySwapChainSupport(physicalDevice);
  bool swapchainAdequate = true;

  if (swapchainSupport.formatsCount <= 0)      { swapchainAdequate = false; }
  if (swapchainSupport.presentModesCount <= 0) { swapchainAdequate = false; }

  // NOTE(WSWhitehouse): Ensure swapchain support arrays have been freed before returning!
  mem_free(swapchainSupport.formats);
  mem_free(swapchainSupport.presentModes);

  if (!swapchainAdequate) return false;

  return true;
}

static b8 CheckPhysicalDeviceExtensionSupport(const VkPhysicalDevice physicalDevice)
{
  u32 availableExtensionCount;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionCount, nullptr);

  VkExtensionProperties* availableExtensions = (VkExtensionProperties*)(mem_alloc(sizeof(VkExtensionProperties) * availableExtensionCount));
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionCount, availableExtensions);

  LOG_INFO("Checking Physical Device Extension Support:");
  const u32 requiredExtensionCount = ARRAY_SIZE(deviceExtensions);
  for (u32 i = 0; i < requiredExtensionCount; i++)
  {
    const char* reqExt = deviceExtensions[i];

    b8 found = false;
    for (u32 j = 0; j < availableExtensionCount; j++)
    {
      if (str_equal(reqExt, availableExtensions[j].extensionName))
      {
        found = true;
        LOG_INFO("\t %s : FOUND", reqExt);
        break;
      }
    }

    if (!found)
    {
      mem_free(availableExtensions);
      LOG_INFO("\t %s : NOT FOUND", reqExt);
      return false;
    }
  }

  mem_free(availableExtensions);
  return true;
}

static vk::SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice physicalDevice)
{
  vk::SwapChainSupportDetails details = {};

  // Surface Capabilities
  VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

  VK_SUCCESS_CHECK(result);

  // Surface Formats
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &details.formatsCount, nullptr);

  if (details.formatsCount != 0)
  {
    details.formats = static_cast<VkSurfaceFormatKHR*>(mem_alloc(sizeof(VkSurfaceFormatKHR) * details.formatsCount));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &details.formatsCount, details.formats);
  }

  // Present Modes
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &details.presentModesCount, nullptr);

  if (details.presentModesCount != 0)
  {
    details.presentModes = static_cast<VkPresentModeKHR*>(mem_alloc(sizeof(VkPresentModeKHR) * details.presentModesCount));
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &details.presentModesCount, details.presentModes);
  }

  return details;
}

static VkSampleCountFlagBits GetMaxSampleCount(const VkPhysicalDevice physicalDevice)
{
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  VkSampleCountFlags sampleCounts = properties.limits.framebufferColorSampleCounts &
                                    properties.limits.framebufferDepthSampleCounts;

  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_64_BIT)) return VK_SAMPLE_COUNT_64_BIT;
  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_32_BIT)) return VK_SAMPLE_COUNT_32_BIT;
  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_16_BIT)) return VK_SAMPLE_COUNT_16_BIT;
  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_8_BIT))  return VK_SAMPLE_COUNT_8_BIT;
  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_4_BIT))  return VK_SAMPLE_COUNT_4_BIT;
  if (BITFLAG_HAS_FLAG(sampleCounts, VK_SAMPLE_COUNT_2_BIT))  return VK_SAMPLE_COUNT_2_BIT;

  return VK_SAMPLE_COUNT_1_BIT;
}

static b8 CreateLogicalDevice()
{
  LOG_INFO("Creating Logical Device...");

  // Find all unique family indices
  // TODO(WSWhitehouse): This is a rather horrible way of getting unique family indices... Rethink?
  u32 uniqueFamilyIndicesCount = 1;
  u32 uniqueFamilyIndices[32];
  mem_zero(&uniqueFamilyIndices, sizeof(u32) * 32);

  uniqueFamilyIndices[0] = device.queueFamilyIndices.graphicsFamily;

  b8 indexFound = false;
  for (u32 i = 0; i < uniqueFamilyIndicesCount; i++)
  {
    if (uniqueFamilyIndices[i] == device.queueFamilyIndices.presentFamily)
    {
      indexFound = true;
      break;
    }
  }

  if (!indexFound)
  {
    uniqueFamilyIndices[uniqueFamilyIndicesCount] = device.queueFamilyIndices.presentFamily;
    uniqueFamilyIndicesCount++;
  }

  indexFound = false;
  for (u32 i = 0; i < uniqueFamilyIndicesCount; i++)
  {
    if (uniqueFamilyIndices[i] == device.queueFamilyIndices.computeFamily)
    {
      indexFound = true;
      break;
    }
  }

  if (!indexFound)
  {
    uniqueFamilyIndices[uniqueFamilyIndicesCount] = device.queueFamilyIndices.computeFamily;
    uniqueFamilyIndicesCount++;
  }

  // Create queue create infos for all unique queue families
  VkDeviceQueueCreateInfo queueCreateInfos[32];
  mem_zero(&queueCreateInfos, sizeof(VkDeviceQueueCreateInfo) * 32);

  f32 queuePriority = 1.0f;
  for (u32 i = 0; i < uniqueFamilyIndicesCount; i++)
  {
    queueCreateInfos[i].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[i].queueFamilyIndex = uniqueFamilyIndices[i];
    queueCreateInfos[i].queueCount       = 1;
    queueCreateInfos[i].pQueuePriorities = &queuePriority;
  }

  VkPhysicalDeviceFeatures2 deviceFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

  // Enable the core built-in features...
  VkPhysicalDeviceFeatures& features = deviceFeatures.features;
  features.samplerAnisotropy = VK_TRUE;
  features.fillModeNonSolid  = VK_TRUE;
  features.wideLines         = VK_TRUE;

  // Enable vulkan 1.2 features...
  VkPhysicalDeviceVulkan12Features vk12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  vk12Features.shaderInt8                        = VK_TRUE;
  vk12Features.storageBuffer8BitAccess           = VK_TRUE;
  vk12Features.uniformAndStorageBuffer8BitAccess = VK_TRUE;

  deviceFeatures.pNext = &vk12Features;

  // Enable atomic float extension features...
  VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};
  atomicFloatFeatures.shaderImageFloat32Atomics   = VK_TRUE;
  atomicFloatFeatures.shaderImageFloat32AtomicAdd = VK_TRUE;

  vk12Features.pNext = &atomicFloatFeatures;

  // Create logical device
  VkDeviceCreateInfo createInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  createInfo.pNext                 = &deviceFeatures;
  createInfo.pQueueCreateInfos     = queueCreateInfos;
  createInfo.queueCreateInfoCount  = uniqueFamilyIndicesCount;

  // NOTE(WSWhitehouse): Using the `VkPhysicalDeviceFeatures2` struct instead which adds
  // support for extension features. This is added in `createInfo.pNext` instead...
  createInfo.pEnabledFeatures = nullptr;

  createInfo.enabledExtensionCount   = ARRAY_SIZE(deviceExtensions);
  createInfo.ppEnabledExtensionNames = deviceExtensions;

#if VK_VALIDATION
  createInfo.enabledLayerCount     = ARRAY_SIZE(validationLayers);
  createInfo.ppEnabledLayerNames   = validationLayers;
#else
  createInfo.enabledLayerCount     = 0;
  createInfo.ppEnabledLayerNames   = nullptr;
#endif

  VK_SUCCESS_CHECK(vkCreateDevice(device.physicalDevice, &createInfo, nullptr, &device.logicalDevice));
  LOG_INFO("Logical Device Created!");

  LOG_INFO("Retrieving Queue Handles...");
  vkGetDeviceQueue(device.logicalDevice, device.queueFamilyIndices.graphicsFamily, 0, &device.graphicsQueue);
  vkGetDeviceQueue(device.logicalDevice, device.queueFamilyIndices.presentFamily, 0, &device.presentQueue);
  vkGetDeviceQueue(device.logicalDevice, device.queueFamilyIndices.computeFamily, 0, &device.computeQueue);
  LOG_INFO("Queue Handles Retrieved!");

  return true;
}

static VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const VkSurfaceFormatKHR* formats, const u32 formatsCount)
{
  for (u32 i = 0; i < formatsCount; i++)
  {
    VkSurfaceFormatKHR format = formats[i];

    if (format.format     != VK_FORMAT_B8G8R8A8_SRGB)           continue;
    if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) continue;

    return format;
  }

  // NOTE(WSWhitehouse): If we didn't find a preferred format, just return the first one
  return formats[0];
}

static VkPresentModeKHR ChooseSwapChainPresentMode(const VkPresentModeKHR* presentModes, const u32 presentModesCount)
{
  for (u32 i = 0; i < presentModesCount; i++)
  {
    VkPresentModeKHR present_mode = presentModes[i];

    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return present_mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR capabilities)
{
  if (capabilities.currentExtent.width != U32_MAX)
  {
    return capabilities.currentExtent;
  }

  u32 width  = (u32)Window::GetWidth();
  u32 height = (u32)Window::GetHeight();

  VkExtent2D extent = { width, height };
  extent.width      = CLAMP(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  extent.height     = CLAMP(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return extent;
}

static void CreateSwapChain()
{
  LOG_INFO("Creating Swapchain...");

  vk::SwapChainSupportDetails supportDetails = QuerySwapChainSupport(device.physicalDevice);

  VkSurfaceFormatKHR surfaceFormat = ChooseSwapChainSurfaceFormat(supportDetails.formats, supportDetails.formatsCount);
  VkPresentModeKHR presentMode     = ChooseSwapChainPresentMode(supportDetails.presentModes, supportDetails.presentModesCount);
  VkExtent2D extent                = ChooseSwapChainExtent(supportDetails.capabilities);

  u32 imageCount = supportDetails.capabilities.minImageCount + 1;

  // NOTE(WSWhitehouse): 0 is a special number that indicates that there is no max image count. If there is a max, clamp
  // the image count between the min and max to ensure its valid.
  if (supportDetails.capabilities.maxImageCount > 0)
  {
    imageCount = CLAMP(imageCount, supportDetails.capabilities.minImageCount, supportDetails.capabilities.maxImageCount);
  }

  // Fill out create info
  VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  createInfo.surface          = surface;
  createInfo.minImageCount    = imageCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  const u32 queueFamilyIndices[] =
    {
      device.queueFamilyIndices.graphicsFamily,
      device.queueFamilyIndices.presentFamily
    };

  if (device.queueFamilyIndices.graphicsFamily != device.queueFamilyIndices.presentFamily)
  {
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = queueFamilyIndices;
  }
  else
  {
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;
  }

  createInfo.preTransform   = supportDetails.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode    = presentMode;
  createInfo.clipped        = VK_TRUE;
  createInfo.oldSwapchain   = VK_NULL_HANDLE;

  // Create swapchain
  VK_SUCCESS_CHECK(vkCreateSwapchainKHR(device.logicalDevice, &createInfo, nullptr, &swapchain.swapchain));
  swapchain.imageFormat = surfaceFormat.format;
  swapchain.extent      = extent;

  LOG_INFO("Swap Chain Created!");

  // NOTE(WSWhitehouse): Free the support details arrays before returning
  mem_free(supportDetails.formats);
  mem_free(supportDetails.presentModes);
}

static void CreateSwapChainImages()
{
  // Get swapchain images
  LOG_INFO("Getting Swap Chain Images...");

  VK_SUCCESS_CHECK(vkGetSwapchainImagesKHR(device.logicalDevice, swapchain.swapchain, &swapchain.imagesCount, nullptr));
  swapchain.images = (VkImage*)mem_alloc(sizeof(VkImage) * swapchain.imagesCount);
  VK_SUCCESS_CHECK(vkGetSwapchainImagesKHR(device.logicalDevice, swapchain.swapchain, &swapchain.imagesCount, swapchain.images));

  LOG_INFO("Swap Chain Images Acquired!");

  // Get swapchain image views
  LOG_INFO("Getting Swap Chain Image Views...");
  swapchain.imageViews = (VkImageView*)mem_alloc(sizeof(VkImageView) * swapchain.imagesCount);

  for (u32 i = 0; i < swapchain.imagesCount; i++)
  {
    VkImageViewCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create_info.image    = swapchain.images[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format   = swapchain.imageFormat;

    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel   = 0;
    create_info.subresourceRange.levelCount     = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount     = 1;

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &create_info, nullptr, &swapchain.imageViews[i]));
  }

  LOG_INFO("Swap Chain Image Views Created!");
}

static void RecreateSwapChain()
{
  windowResize = false;

  LOG_INFO("Swapchain recreation has been called...");

  int width  = Window::GetWidth();
  int height = Window::GetHeight();

  while(width == 0 || height == 0)
  {
    Window::WaitMessages();
    windowResize = false;

    width  = Window::GetWidth();
    height = Window::GetHeight();
  }

  Renderer::WaitForDeviceIdle();

  DestroySwapChain();

  // TODO(WSWhitehouse): Perform checks when the swapchain is being recreated...
  CreateSwapChain();
  CreateSwapChainImages();
  CreateDepthBuffer();

  UI::RecreateSwapchain();

  CreateFramebuffers();
  ImGuiRenderer::RecreateSwapchain();
}

static void CreateFramebuffers()
{
  swapchain.framebuffers = (VkFramebuffer*)(mem_alloc(sizeof(VkFramebuffer) * swapchain.imagesCount));

  for (u32 i = 0; i < swapchain.imagesCount; i++)
  {
    VkImageView attachments[] =
      {
        swapchain.imageViews[i],
        swapchain.depthImageViews[i],
        uiImageView
      };

    VkFramebufferCreateInfo framebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferCreateInfo.renderPass      = renderPass;
    framebufferCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
    framebufferCreateInfo.pAttachments    = attachments;
    framebufferCreateInfo.width           = swapchain.extent.width;
    framebufferCreateInfo.height          = swapchain.extent.height;
    framebufferCreateInfo.layers          = 1;

    VK_SUCCESS_CHECK(vkCreateFramebuffer(device.logicalDevice, &framebufferCreateInfo, nullptr, &swapchain.framebuffers[i]));
  }
}

static b8 CreateRenderPass()
{
  const VkAttachmentDescription attachments[] =
    {
      // 0 = Colour Attachment
      {
        .format         = swapchain.imageFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },

      // 1 = Depth Attachment
      {
        .format         = swapchain.depthFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },

      // 2 = UI Render Target Attachment
      {
        .format         = UI_IMAGE_FORMAT,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      }
    };

  VkAttachmentReference colourAttachmentRef = {};
  colourAttachmentRef.attachment = 0;
  colourAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef = {};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference uiAttachmentRef = {};
  uiAttachmentRef.attachment = 2;
  uiAttachmentRef.layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  const VkSubpassDescription subpasses[] =
    {
      // main subpass
      {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colourAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
      },

      // ui blit subpass
      {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 1,
        .pInputAttachments       = &uiAttachmentRef,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colourAttachmentRef,
        .pDepthStencilAttachment = nullptr,
      }
    };

  const VkSubpassDependency dependencies[] =
    {
      // main subpass
      {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      },

      // ui blit subpass
      {
        .srcSubpass    = 0,
        .dstSubpass    = 1,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      }
    };

  VkRenderPassCreateInfo renderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
  renderPassCreateInfo.pAttachments    = attachments;
  renderPassCreateInfo.subpassCount    = ARRAY_SIZE(subpasses);
  renderPassCreateInfo.pSubpasses      = subpasses;
  renderPassCreateInfo.dependencyCount = ARRAY_SIZE(dependencies);
  renderPassCreateInfo.pDependencies   = dependencies;

  VK_SUCCESS_CHECK(vkCreateRenderPass(device.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass));
  return true;
}

static b8 CreateCommandPools()
{
  LOG_INFO("CreateECS Command Pools...");

  if (!graphicsCommandPool.Create(device, device.queueFamilyIndices.graphicsFamily, device.graphicsQueue))
  {
    LOG_ERROR("Failed to create graphics command pool!");
    return false;
  }

  if (!computeCommandPool.Create(device, device.queueFamilyIndices.computeFamily, device.computeQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT))
  {
    LOG_ERROR("Failed to create compute command pool!");
    return false;
  }

  return true;
}

static b8 CreateCommandBuffer()
{
  LOG_INFO("CreateECS command buffer...");

  VkCommandBufferAllocateInfo bufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  bufferAllocateInfo.commandPool        = graphicsCommandPool.pool;
  bufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandBufferCount = commandBuffer.Size();

  VK_SUCCESS_CHECK(vkAllocateCommandBuffers(device.logicalDevice, &bufferAllocateInfo, commandBuffer.data));
  return true;
}

static b8 CreateDepthBuffer()
{
  LOG_INFO("Creating Depth Buffer...");

  if (!FindDepthFormat(device, &swapchain.depthFormat))
  {
    LOG_FATAL("Failed to find suitable depth format!");
    return false;
  }

  swapchain.depthImages     = (vk::Image*)mem_alloc(sizeof(vk::Image) * swapchain.imagesCount);
  swapchain.depthImageViews = (VkImageView*)mem_alloc(sizeof(VkImageView) * swapchain.imagesCount);

  for (u32 i = 0; i < swapchain.imagesCount; ++i)
  {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.extent      =
      {
        .width  = swapchain.extent.width,
        .height = swapchain.extent.height,
        .depth  = 1
      };
    imageCreateInfo.format        = swapchain.depthFormat;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags         = 0;

    swapchain.depthImages[i].Create(device, &imageCreateInfo);

    VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.image    = swapchain.depthImages[i].image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format   = swapchain.depthFormat;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &imageViewCreateInfo, nullptr, &swapchain.depthImageViews[i]));
  }

  LOG_INFO("Depth Buffer Created!");
  return true;
}

static void CreateDescriptorPool()
{
  LOG_INFO("Creating Descriptor Pool...");

  // REVIEW(WSWhitehouse): I have no idea if created a pool for each type is a good idea... but it works!

  const u32 POOL_SIZE = 300;
  const VkDescriptorPoolSize poolSizes[] =
    {
      { VK_DESCRIPTOR_TYPE_SAMPLER,                POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, POOL_SIZE },
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       POOL_SIZE }
    };

  VkDescriptorPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolCreateInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolCreateInfo.maxSets       = ARRAY_SIZE(poolSizes) * 100;
  poolCreateInfo.poolSizeCount = ARRAY_SIZE(poolSizes);
  poolCreateInfo.pPoolSizes    = poolSizes;

  VK_SUCCESS_CHECK(vkCreateDescriptorPool(device.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool));

  LOG_INFO("Creating Descriptor Pool!");
}

static void CreateCoreDescriptorsAndBuffers()
{
  VkDescriptorSetLayoutBinding frameDataLayoutBinding  = UBOFrameData::GetDescriptorSetLayoutBinding();
  VkDescriptorSetLayoutBinding cameraDataLayoutBinding = UBOCameraData::GetDescriptorSetLayoutBinding();

  VkDescriptorSetLayoutBinding bindings[] =
    {
      frameDataLayoutBinding,
      cameraDataLayoutBinding,
    };

  VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
  layoutCreateInfo.pBindings    = bindings;

  VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &coreDescriptorSetLayout));


  VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {};
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    // Set descriptor set layout
    layouts[i] = coreDescriptorSetLayout;

    // Create Frame Data Buffer
    {
      const b8 success = frameDataUBO[i].Create(device, sizeof(UBOFrameData),
                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success) ABORT(ABORT_CODE_VK_FAILURE);

      frameDataUBO[i].MapMemory(device, (void**)&frameDataUBOMapped[i]);
    }

    // Create Camera Data Buffer
    {
      const b8 success = cameraDataUBO[i].Create(device, sizeof(UBOCameraData),
                                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      if (!success) ABORT(ABORT_CODE_VK_FAILURE);

      cameraDataUBO[i].MapMemory(device, (void**)&cameraDataUBOMapped[i]);
    }
  }

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  descriptorSetAllocateInfo.descriptorPool     = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  descriptorSetAllocateInfo.pSetLayouts        = layouts;

  VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice, &descriptorSetAllocateInfo, coreDescriptorSet.data));

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    // Frame Data UBO
    VkDescriptorBufferInfo frameDataBufferInfo =
      UBOFrameData::GetDescriptorBufferInfo(frameDataUBO[i].buffer);

    VkWriteDescriptorSet frameDataDescriptorWrite =
      UBOFrameData::GetWriteDescriptorSet(coreDescriptorSet[i], &frameDataBufferInfo);

    // Camera Data UBO
    VkDescriptorBufferInfo cameraDataBufferInfo =
      UBOCameraData::GetDescriptorBufferInfo(cameraDataUBO[i].buffer);

    VkWriteDescriptorSet cameraDataDescriptorWrite =
      UBOCameraData::GetWriteDescriptorSet(coreDescriptorSet[i], &cameraDataBufferInfo);

    VkWriteDescriptorSet descriptorWrites[] =
      {
        frameDataDescriptorWrite,
        cameraDataDescriptorWrite,
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }
}

static void DestroyCoreDescriptorsAndBuffers()
{
  // Destroy core uniform buffers
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    frameDataUBO[i].UnmapMemory(device);
    cameraDataUBO[i].UnmapMemory(device);

    frameDataUBO[i].Destroy(device);
    cameraDataUBO[i].Destroy(device);
  }

  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), MAX_FRAMES_IN_FLIGHT, coreDescriptorSet.data);

  // Destroy pipeline agnostic descriptor set layouts
  vkDestroyDescriptorSetLayout(device.logicalDevice, coreDescriptorSetLayout, nullptr);
}

