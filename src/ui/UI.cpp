#include "ui/UI.hpp"

//core
#include "core/Window.hpp"
#include "core/Assert.hpp"

// filesystem
#include "filesystem/AssetDatabase.hpp"
#include "filesystem/FileSystem.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"
#include "renderer/Renderer.hpp"

// geometry
#include "geometry/Vertex.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/components/UIImage.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/ComponentFactory.hpp"

// --- UI RENDER PASS --- //
static VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
static VkRenderPass renderPass         = VK_NULL_HANDLE;
static VkCommandBuffer commandBuffer   = VK_NULL_HANDLE;

// --- SYNC OBJECTS --- //
VkSemaphore uiRenderFinishedSemaphore = VK_NULL_HANDLE;
static VkFence uiInFlightFence        = VK_NULL_HANDLE;

// --- UI RENDER PIPELINE --- //
static struct
{
  VkDescriptorSetLayout descriptorSetLayout;
  VkPipelineLayout layout;
  VkPipeline pipeline;
} pipeline = {};

// UI Data UBO
static VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
static vk::Buffer dataUniformBuffer  = {};
static void* dataUniformBufferMapped = {nullptr};

static VkDescriptorSetLayout imageDescriptorSetLayout = VK_NULL_HANDLE;

// --- UI RENDER TARGET --- //
vk::Image uiImage;
VkImageView uiImageView;

static vk::Image uiDepthImage;
static VkImageView uiDepthImageView;

static VkFormat uiDepthFormat;
static VkExtent2D uiImageExtent;

static VkFramebuffer uiFramebuffer;

// --- BLIT PIPELINE --- //
static struct
{
  VkDescriptorSetLayout descriptorSetLayout;
  VkPipelineLayout layout;
  VkPipeline pipeline;
} blitPipeline = {};
static VkDescriptorSet blitDescriptorSet = VK_NULL_HANDLE;

static b8 isDirty             = false;
static b8 hasRedrawnThisFrame = false;

// TODO(WSWhitehouse): Move to vertex shader, there is no need to upload quad data.
extern MeshBufferData quadMesh;

// Forward Declarations
static void CreateRenderPass();
static void CreateFramebuffers();
static void DestroyFramebuffers();
static void CreateBlitPipeline();
static void UpdateBlitDescriptorSet();
static void CreateUIElementPipeline();

struct UniformBufferUIData
{
  alignas(8) glm::vec2 screenSize;

  // --- Utility Functions --- //
  static INLINE VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding()
  {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding            = 0;
    layoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount    = 1;
    layoutBinding.pImmutableSamplers = nullptr;
    layoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    return layoutBinding;
  }
};

struct PushConstantData
{
  alignas(16) glm::vec3 colour;
  alignas(8)  glm::vec2 pos;
  alignas(8)  glm::vec2 size;
  alignas(8)  glm::vec2 scale;
  alignas(4)  f32 zOrder;
  alignas(4)  f32 texIndex;
};

void UI::Init()
{
  const vk::Device& device = Renderer::GetDevice();

  // Create sync objects...
  {
    VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_SUCCESS_CHECK(vkCreateSemaphore(device.logicalDevice, &semaphoreInfo, nullptr, &uiRenderFinishedSemaphore));
    VK_SUCCESS_CHECK(vkCreateFence(device.logicalDevice, &fenceInfo, nullptr, &uiInFlightFence));
  }

  // Create command buffer...
  {
    VkCommandBufferAllocateInfo bufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferAllocateInfo.commandPool        = Renderer::GetGraphicsCommandPool().pool;
    bufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocateInfo.commandBufferCount = 1;

    VK_SUCCESS_CHECK(vkAllocateCommandBuffers(device.logicalDevice, &bufferAllocateInfo, &commandBuffer));
  }

  // Create descriptor pool...
  {
    const u32 POOL_SIZE = 32;
    VkDescriptorPoolSize poolSizes[] =
      {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, POOL_SIZE },
//        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       POOL_SIZE }
      };

    VkDescriptorPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolCreateInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets       = 100;
    poolCreateInfo.poolSizeCount = ARRAY_SIZE(poolSizes);
    poolCreateInfo.pPoolSizes    = poolSizes;

    VK_SUCCESS_CHECK(vkCreateDescriptorPool(device.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool));
  }

  CreateRenderPass();
  CreateFramebuffers();
  CreateBlitPipeline();
  UpdateBlitDescriptorSet();
  CreateUIElementPipeline();

  UI::SetDirty();
}

void UI::Shutdown()
{
  const vk::Device& device = Renderer::GetDevice();

  // NOTE(WSWhitehouse): Wait for work to complete...
  vkWaitForFences(device.logicalDevice, 1, &uiInFlightFence, VK_TRUE, U64_MAX);

  dataUniformBuffer.Destroy(device);

  // Free Descriptor Sets
  vkFreeDescriptorSets(device.logicalDevice, descriptorPool, 1, &descriptorSet);
  vkFreeDescriptorSets(device.logicalDevice, Renderer::GetDescriptorPool(), 1, &blitDescriptorSet);

  // Destroy Descriptor Set Layouts
  vkDestroyDescriptorSetLayout(device.logicalDevice, imageDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(device.logicalDevice, pipeline.descriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(device.logicalDevice, blitPipeline.descriptorSetLayout, nullptr);

  // Destroy Pipeline
  vkDestroyPipelineLayout(device.logicalDevice, pipeline.layout, nullptr);
  vkDestroyPipelineLayout(device.logicalDevice, blitPipeline.layout, nullptr);
  vkDestroyPipeline(device.logicalDevice, pipeline.pipeline, nullptr);
  vkDestroyPipeline(device.logicalDevice, blitPipeline.pipeline, nullptr);

  DestroyFramebuffers();

  vkDestroyRenderPass(device.logicalDevice, renderPass, nullptr);
  vkDestroyDescriptorPool(device.logicalDevice, descriptorPool, nullptr);
  vkFreeCommandBuffers(device.logicalDevice, Renderer::GetGraphicsCommandPool().pool, 1, &commandBuffer);
  vkDestroySemaphore(device.logicalDevice, uiRenderFinishedSemaphore, nullptr);
  vkDestroyFence(device.logicalDevice, uiInFlightFence, nullptr);
}

void UI::RecreateSwapchain()
{
  const vk::Device& device = Renderer::GetDevice();

  // NOTE(WSWhitehouse): Wait for work to complete...
  vkWaitForFences(device.logicalDevice, 1, &uiInFlightFence, VK_TRUE, U64_MAX);

  DestroyFramebuffers();
  CreateFramebuffers();

  // Update blit descriptor set as it uses uiImage as an input attachment...
  UpdateBlitDescriptorSet();

  // NOTE(WSWhitehouse): As the framebuffers and images have been recreated, the UI
  // will need to be redrawn again in the next frame!
  SetDirty();
}

void UI::SetDirty()
{
  isDirty = true;
}

b8 UI::HasRedrawnThisFrame()
{
  return hasRedrawnThisFrame;
}

void UI::ResetHasRedrawnThisFrame()
{
  hasRedrawnThisFrame = false;
}

void UI::DrawUI(ECS::Manager& ecs)
{
  using namespace ECS;

  // NOTE(WSWhitehouse): If the UI is not dirty, don't redraw it.
  if (!isDirty) return;

  hasRedrawnThisFrame = true;

  const vk::Device& device = Renderer::GetDevice();

  vkWaitForFences(device.logicalDevice, 1, &uiInFlightFence, VK_TRUE, U64_MAX);

  // TODO(WSWhitehouse): Check for swapchain recreation...

  vkResetFences(device.logicalDevice, 1, &uiInFlightFence);

  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags            = 0;
  beginInfo.pInheritanceInfo = nullptr;

  VK_SUCCESS_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  // NOTE(WSWhitehouse): MSVC complains about inlining the offset variable when setting
  // up the render pass begin info and the scissor...
  VkOffset2D offset = {0,0};

  constexpr const VkClearValue colourClearVal = { .color = {{0.0f, 0.0f, 0.0f, 0.0f}} };
  constexpr const VkClearValue depthClearVal  = { .depthStencil = {1.0f, 0} };
  constexpr const VkClearValue clearValues[]  =
    {
      colourClearVal,
      depthClearVal
    };

  VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass        = renderPass;
  renderPassInfo.framebuffer       = uiFramebuffer;
  renderPassInfo.renderArea.offset = offset;
  renderPassInfo.renderArea.extent = uiImageExtent;
  renderPassInfo.clearValueCount   = ARRAY_SIZE(clearValues);
  renderPassInfo.pClearValues      = clearValues;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {};
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = (f32)uiImageExtent.width;
  viewport.height   = (f32)uiImageExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset   = offset;
  scissor.extent   = uiImageExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

  UniformBufferUIData uiData = {};
  uiData.screenSize = {(f32)uiImageExtent.width, (f32)uiImageExtent.height};
  mem_copy(dataUniformBufferMapped, &uiData, sizeof(UniformBufferUIData));

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.layout, 0, 1, &descriptorSet, 0, nullptr);

  VkDeviceSize quadVertOffsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &quadMesh.vertexBuffer.buffer, quadVertOffsets);
  vkCmdBindIndexBuffer(commandBuffer, quadMesh.indexBuffer.buffer, 0, quadMesh.indexType);

  ComponentSparseSet* imageSparseSet = ecs.GetComponentSparseSet<UIImage>();
  for (u32 imageIndex = 0; imageIndex < imageSparseSet->componentCount; ++imageIndex)
  {
    ComponentData<UIImage>& componentData = ((ComponentData<UIImage>*)imageSparseSet->componentArray)[imageIndex];
    UIImage& image = componentData.component;

    if (!image.render) continue;

    const PushConstantData vertPushConst =
    {
      .colour   = image.colour,
      .pos      = image.pos,
      .size     = image.size,
      .scale    = image.scale,
      .zOrder   = CLAMP(image.zOrder, 0.0f, 1.0f - F32_EPSILON),
      .texIndex = (f32)image.currentTexIndex
    };

    vkCmdPushConstants(commandBuffer, pipeline.layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstantData), &vertPushConst);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.layout, 1, 1, &image.descriptorSets, 0, nullptr);

    // Render quad...
    vkCmdDrawIndexed(commandBuffer, quadMesh.indexCount, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(commandBuffer);
  VK_SUCCESS_CHECK(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount   = 0;
  submitInfo.pWaitSemaphores      = nullptr;
  submitInfo.pWaitDstStageMask    = nullptr;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &uiRenderFinishedSemaphore;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &commandBuffer;

  VK_SUCCESS_CHECK(vkQueueSubmit(device.graphicsQueue, 1, &submitInfo, uiInFlightFence));
  isDirty = false;
}

void UI::BlitUI(VkCommandBuffer cmdBuffer)
{
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blitPipeline.pipeline);

  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blitPipeline.layout, 0, 1, &blitDescriptorSet, 0, nullptr);

  vkCmdDraw(cmdBuffer, 3, 1, 0, 0); // Fullscreen Quad
}

static void CreateRenderPass()
{
  const vk::Device& device = Renderer::GetDevice();
  const vk::Swapchain& swapchain = Renderer::GetSwapchain();

  uiDepthFormat = swapchain.depthFormat;

  const VkAttachmentDescription attachments[] =
    {
      // 0 = Colour Attachment
      {
        .format         = UI_IMAGE_FORMAT,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      },

      // 1 = Depth Attachment
      {
        .format         = uiDepthFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      }
    };

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef = {};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount    = 1;
  subpass.pColorAttachments       = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass    = 0;
  dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT  |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
  renderPassCreateInfo.pAttachments    = attachments;
  renderPassCreateInfo.subpassCount    = 1;
  renderPassCreateInfo.pSubpasses      = &subpass;
  renderPassCreateInfo.dependencyCount = 1;
  renderPassCreateInfo.pDependencies   = &dependency;

  VK_SUCCESS_CHECK(vkCreateRenderPass(device.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass));
}

static void CreateFramebuffers()
{
  const vk::Device& device       = Renderer::GetDevice();
  const vk::Swapchain& swapchain = Renderer::GetSwapchain();

  uiImageExtent = swapchain.extent;

  // Create ui render target image...
  {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.extent      =
      {
        .width  = uiImageExtent.width,
        .height = uiImageExtent.height,
        .depth  = 1
      };
    imageCreateInfo.format        = UI_IMAGE_FORMAT;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags         = 0;

    if (!uiImage.Create(device, &imageCreateInfo)) ABORT(ABORT_CODE_VK_FAILURE);
  }

  // Create ui render target image view...
  {
    VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image            = uiImage.image;
    createInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format           = UI_IMAGE_FORMAT;
    createInfo.subresourceRange =
      {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
      };

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &createInfo, nullptr, &uiImageView));
  }

  // Create ui depth buffer image...
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
    imageCreateInfo.format        = uiDepthFormat;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.flags         = 0;

    uiDepthImage.Create(device, &imageCreateInfo);
  }

  // Create ui depth buffer image view...
  {
    VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.image    = uiDepthImage.image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format   = uiDepthFormat;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VK_SUCCESS_CHECK(vkCreateImageView(device.logicalDevice, &imageViewCreateInfo, nullptr, &uiDepthImageView));
  }

  // Create ui render target frame buffer...
  {
    VkImageView attachments[] =
      {
        uiImageView,
        uiDepthImageView
      };

    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass      = renderPass;
    createInfo.attachmentCount = ARRAY_SIZE(attachments);
    createInfo.pAttachments    = attachments;
    createInfo.width           = uiImageExtent.width;
    createInfo.height          = uiImageExtent.height;
    createInfo.layers          = 1;

    vkCreateFramebuffer(device.logicalDevice, &createInfo, nullptr, &uiFramebuffer);
  }
}

static void DestroyFramebuffers()
{
  const vk::Device& device = Renderer::GetDevice();

  uiImage.Destroy(device);
  vkDestroyImageView(device.logicalDevice, uiImageView, nullptr);

  uiDepthImage.Destroy(device);
  vkDestroyImageView(device.logicalDevice, uiDepthImageView, nullptr);

  vkDestroyFramebuffer(device.logicalDevice, uiFramebuffer, nullptr);
}

static void CreateBlitPipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  LOG_INFO("\tCreating UI Blit Graphics Pipeline...");

  // Create Descriptor Set Layout
  {
    VkDescriptorSetLayoutBinding uiImageDescriptorSetLayout = {};
    uiImageDescriptorSetLayout.binding            = 0;
    uiImageDescriptorSetLayout.descriptorType     = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    uiImageDescriptorSetLayout.descriptorCount    = 1;
    uiImageDescriptorSetLayout.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    uiImageDescriptorSetLayout.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bindings[] =
      {
        uiImageDescriptorSetLayout
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &blitPipeline.descriptorSetLayout));
  }

  // Create Graphics Pipeline
  {
    // Load shader code from file
    FileSystem::FileContent vertShaderCode, fragShaderCode;

    if (!FileSystem::ReadAllFileContent("shaders/fullscreen.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
    if (!FileSystem::ReadAllFileContent("shaders/ui/uiBlit.frag.spv", &fragShaderCode))  ABORT(ABORT_CODE_ASSET_FAILURE);

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

    // Dynamic State
    VkDynamicState dynamicStates[] =
      {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = ARRAY_SIZE(dynamicStates);
    dynamicState.pDynamicStates    = dynamicStates;

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor
    // NOTE(WSWhitehouse): We defined the viewport and scissor as a dynamic state above, so no need to define them here.
    // It will be done at draw time instead. https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

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
    depthStencil.depthTestEnable       = VK_FALSE;
    depthStencil.depthWriteEnable      = VK_FALSE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds        = 0.0f;
    depthStencil.maxDepthBounds        = 1.0f;
    depthStencil.stencilTestEnable     = VK_FALSE;
    depthStencil.front                 = {};
    depthStencil.back                  = {};

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &blitPipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &blitPipeline.layout));

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCreateInfo.stageCount          = 2;
    pipelineCreateInfo.pStages             = shaderStages;
    pipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState      = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState   = &multisampling;
    pipelineCreateInfo.pDepthStencilState  = &depthStencil;
    pipelineCreateInfo.pColorBlendState    = &colorBlending;
    pipelineCreateInfo.pDynamicState       = &dynamicState;
    pipelineCreateInfo.layout              = blitPipeline.layout;
    pipelineCreateInfo.renderPass          = Renderer::GetRenderpass();
    pipelineCreateInfo.subpass             = 1;

    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex  = -1;

    VK_SUCCESS_CHECK(vkCreateGraphicsPipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                               &pipelineCreateInfo, nullptr, &blitPipeline.pipeline));

    // Destroy shader modules
    vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

    // Clean up file content
    mem_free(vertShaderCode.data);
    mem_free(fragShaderCode.data);

    LOG_INFO("\tUI Blit Graphics Pipeline Created!");
  }

  // Allocate blit descriptor set...
  {
    VkDescriptorSetAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool     = Renderer::GetDescriptorPool();
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts        = &blitPipeline.descriptorSetLayout;

    vkAllocateDescriptorSets(device.logicalDevice, &allocateInfo, &blitDescriptorSet);
  }
}

static void UpdateBlitDescriptorSet()
{
  const vk::Device& device = Renderer::GetDevice();

  VkDescriptorImageInfo inputAttachmentInfo = {};
  inputAttachmentInfo.sampler     = VK_NULL_HANDLE;
  inputAttachmentInfo.imageView   = uiImageView;
  inputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet inputAttachmentWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  inputAttachmentWrite.dstSet          = blitDescriptorSet;
  inputAttachmentWrite.dstBinding      = 0;
  inputAttachmentWrite.dstArrayElement = 0;
  inputAttachmentWrite.descriptorType  = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
  inputAttachmentWrite.descriptorCount = 1;
  inputAttachmentWrite.pImageInfo      = &inputAttachmentInfo;

  vkUpdateDescriptorSets(device.logicalDevice, 1, &inputAttachmentWrite, 0, nullptr);
}

static void CreateUIElementPipeline()
{
  const vk::Device& device = Renderer::GetDevice();

  LOG_INFO("\tCreating UI Element Graphics Pipeline...");

  // Create Descriptor Set Layout
  {
    VkDescriptorSetLayoutBinding uiDataBinding = UniformBufferUIData::GetDescriptorSetLayoutBinding();

    VkDescriptorSetLayoutBinding bindings[] =
      {
        uiDataBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
    layoutCreateInfo.pBindings    = bindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &pipeline.descriptorSetLayout));
  }

  // Create Image Descriptor Set Layout
  {
    VkDescriptorSetLayoutBinding textureSamplerLayoutBinding = {};
    textureSamplerLayoutBinding.binding            = 0;
    textureSamplerLayoutBinding.descriptorCount    = 1;
    textureSamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureSamplerLayoutBinding.pImmutableSamplers = nullptr;
    textureSamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding imageBindings[] =
      {
        textureSamplerLayoutBinding
      };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCreateInfo.bindingCount = ARRAY_SIZE(imageBindings);
    layoutCreateInfo.pBindings    = imageBindings;

    VK_SUCCESS_CHECK(vkCreateDescriptorSetLayout(device.logicalDevice, &layoutCreateInfo, nullptr, &imageDescriptorSetLayout));
  }

  // Create Graphics Pipeline
  {
    // Load shader code from file
    FileSystem::FileContent vertShaderCode, fragShaderCode;

    if (!FileSystem::ReadAllFileContent("shaders/ui/ui.vert.spv", &vertShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);
    if (!FileSystem::ReadAllFileContent("shaders/ui/ui.frag.spv", &fragShaderCode)) ABORT(ABORT_CODE_ASSET_FAILURE);

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

    // Dynamic State
    VkDynamicState dynamicStates[] =
      {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = ARRAY_SIZE(dynamicStates);
    dynamicState.pDynamicStates    = dynamicStates;

    // Vertex Input
    VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();

    // NOTE(WSWhitehouse): Using auto here so if/when the attribute descriptions
    // array size changes this variable doesn't need to be updated...
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.Size();
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data;

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor
    // NOTE(WSWhitehouse): We defined the viewport and scissor as a dynamic state above, so no need to define them here.
    // It will be done at draw time instead. https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
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
        pipeline.descriptorSetLayout,
        imageDescriptorSetLayout
      };

    const VkPushConstantRange pushConstants[] =
      {
        {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset     = 0,
          .size       = sizeof(PushConstantData)
        }
      };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = ARRAY_SIZE(descriptorSetLayouts);
    pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = ARRAY_SIZE(pushConstants);
    pipelineLayoutInfo.pPushConstantRanges    = pushConstants;

    VK_SUCCESS_CHECK(vkCreatePipelineLayout(device.logicalDevice, &pipelineLayoutInfo, nullptr, &pipeline.layout));

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCreateInfo.stageCount          = 2;
    pipelineCreateInfo.pStages             = shaderStages;
    pipelineCreateInfo.pVertexInputState   = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState      = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState   = &multisampling;
    pipelineCreateInfo.pDepthStencilState  = &depthStencil;
    pipelineCreateInfo.pColorBlendState    = &colorBlending;
    pipelineCreateInfo.pDynamicState       = &dynamicState;
    pipelineCreateInfo.layout              = pipeline.layout;
    pipelineCreateInfo.renderPass          = renderPass;
    pipelineCreateInfo.subpass             = 0;

    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex  = -1;

    VK_SUCCESS_CHECK(vkCreateGraphicsPipelines(device.logicalDevice, VK_NULL_HANDLE, 1,
                                               &pipelineCreateInfo, nullptr, &pipeline.pipeline));

    // Destroy shader modules
    vkDestroyShaderModule(device.logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(device.logicalDevice, fragShaderModule, nullptr);

    // Clean up file content
    mem_free(vertShaderCode.data);
    mem_free(fragShaderCode.data);

    LOG_INFO("\tUI Element Graphics Pipeline Created!");
  }

  // Create Descriptor Set...
  {
    b8 success = dataUniformBuffer.Create(device, sizeof(UniformBufferUIData),
                                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!success) ABORT(ABORT_CODE_VK_FAILURE);

    dataUniformBuffer.MapMemory(device, &dataUniformBufferMapped);

    VkDescriptorSetAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool     = descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts        = &pipeline.descriptorSetLayout;

    vkAllocateDescriptorSets(device.logicalDevice, &allocateInfo, &descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = dataUniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferUIData);

    VkWriteDescriptorSet bufferWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    bufferWrite.dstSet           = descriptorSet;
    bufferWrite.dstBinding       = 0;
    bufferWrite.dstArrayElement  = 0;
    bufferWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bufferWrite.descriptorCount  = 1;
    bufferWrite.pBufferInfo      = &bufferInfo;
    bufferWrite.pImageInfo       = nullptr;
    bufferWrite.pTexelBufferView = nullptr;

    const VkWriteDescriptorSet descriptorWrites[] =
      {
        bufferWrite
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }
}


void ComponentFactory::UIImageCreate(UIImage* image, const UIImageCreateInfo& createInfo)
{
  const vk::Device& device = Renderer::GetDevice();

  b8 imageCreationSuccess;
  if (createInfo.textureCount == 1)
  {
    // Load texture data
    TextureData* texData = AssetDatabase::LoadTexture(createInfo.texturePath[0]);
    if (texData == nullptr) return;

    image->size = glm::vec2(texData->width, texData->height);
    image->textureCount    = 1;
    image->currentTexIndex = 0;

    imageCreationSuccess = image->texture.CreateImageFromRawData(texData);

    // Free texture data
    AssetDatabase::FreeTexture(texData);
  }
  else
  {
    // Load texture data
    TextureData** texData = (TextureData**)mem_alloc(sizeof(TextureData*) * createInfo.textureCount);
    for (u32 i = 0; i < createInfo.textureCount; ++i)
    {
      texData[i] = AssetDatabase::LoadTexture(createInfo.texturePath[i]);
      if (texData[i] == nullptr) return;
    }

    image->size = glm::vec2(texData[0]->width, texData[0]->height);
    image->textureCount    = createInfo.textureCount;
    image->currentTexIndex = 0;

    imageCreationSuccess = image->texture.CreateImageArrayFromRawData(texData, createInfo.textureCount);

    // Free texture data
    for (u32 i = 0; i < createInfo.textureCount; ++i)
    {
      AssetDatabase::FreeTexture(texData[i]);
    }
    mem_free(texData);
  }

  if (!imageCreationSuccess)
  {
    LOG_FATAL("Failed to create UI image!");
    return;
  }

  b8 samplerCreationSuccess = image->texture.CreateSampler(createInfo.samplerFilter);
  if (!samplerCreationSuccess)
  {
    LOG_FATAL("Failed to create image sampler!");
    return;
  }

  // Create Descriptor Sets
  {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool     = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts        = &imageDescriptorSetLayout;

    VK_SUCCESS_CHECK(vkAllocateDescriptorSets(device.logicalDevice,
                                              &descriptorSetAllocateInfo,
                                              &image->descriptorSets));

    // Image info
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = image->texture.imageView;
    imageInfo.sampler     = image->texture.sampler;


    VkWriteDescriptorSet imageDescriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    imageDescriptorWrite.dstSet          = image->descriptorSets;
    imageDescriptorWrite.dstBinding      = 0;
    imageDescriptorWrite.dstArrayElement = 0;
    imageDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageDescriptorWrite.descriptorCount = 1;
    imageDescriptorWrite.pImageInfo      = &imageInfo;

    const VkWriteDescriptorSet descriptorWrites[] =
      {
        imageDescriptorWrite
      };

    vkUpdateDescriptorSets(device.logicalDevice, ARRAY_SIZE(descriptorWrites), descriptorWrites, 0, nullptr);
  }
}

void ComponentFactory::UIImageDestroy(UIImage* image)
{
  const vk::Device& device = Renderer::GetDevice();

  vkFreeDescriptorSets(device.logicalDevice, descriptorPool, 1, &image->descriptorSets);

  image->texture.DestroySampler();
  image->texture.DestroyImage();
}
