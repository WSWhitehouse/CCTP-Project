#ifndef SNOWFLAKE_GRAPHICS_PIPELINE_HPP
#define SNOWFLAKE_GRAPHICS_PIPELINE_HPP

#include "pch.hpp"
#include "renderer/vk/Vulkan.hpp"

// Forward Declarations
namespace ECS { struct Manager; }
struct Camera;

/** @brief The handle that represents a Graphics Pipeline. */
typedef u64 PipelineHandle;

/** @brief An invalid pipeline handle. */
#define INVALID_PIPELINE_HANDLE 0ULL

/** @brief A Vulkan Graphics Pipeline */
struct GraphicsPipeline
{
  /** @brief The function ptr type for rendering a graphics pipeline. */
  typedef void (*RenderFuncPtr)(ECS::Manager& ecs, const Camera& camera, VkCommandBuffer cmdBuffer, u32 currentFrame);
  typedef void (*CleanUpFuncPtr)();

  PipelineHandle handle;

  RenderFuncPtr renderFuncPtr;
  CleanUpFuncPtr cleanUpFuncPtr;

  VkPipelineLayout layout;
  VkPipeline pipeline;
};

/**
* @brief The render queue for the pipeline. Pipelines are
* sorted and rendered in the queue order.
*/
enum GraphicsRenderQueue
{
  OPAQUE_QUEUE      = 0,
  SKYBOX_QUEUE      = 1,
  TRANSPARENT_QUEUE = 2,
  FULL_SCREEN_QUEUE = 3
};

/** @brief Config for creating a Graphics Pipeline */
struct GraphicsPipelineConfig
{
  // Rendering
  GraphicsPipeline::RenderFuncPtr renderFuncPtr   = nullptr;
  GraphicsPipeline::CleanUpFuncPtr cleanUpFuncPtr = nullptr;

  GraphicsRenderQueue renderQueue = OPAQUE_QUEUE;

  u32 renderPass    = 0;
  u32 renderSubpass = 0;

  // Shader Stages
  u32 shaderStageCreateInfoCount = 0;
  const VkPipelineShaderStageCreateInfo* shaderStageCreateInfos = nullptr;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Customisable States...
  const VkPipelineVertexInputStateCreateInfo* vertexInputStateCreateInfo     = nullptr;
  const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo = nullptr;
  const VkPipelineMultisampleStateCreateInfo* multisampleStateCreateInfo     = nullptr;
  const VkPipelineColorBlendStateCreateInfo* colourBlendStateCreateInfo      = nullptr;
  const VkPipelineDepthStencilStateCreateInfo* depthStencilStateCreateInfo   = nullptr;

  // Descriptor Sets
  u32 descriptorSetLayoutCount = 0;
  const VkDescriptorSetLayout* descriptorSetLayouts = nullptr;

  // Push Constants
  u32 pushConstantRangeCount = 0;
  const VkPushConstantRange* pushConstantRanges = nullptr;
};


#endif //SNOWFLAKE_GRAPHICS_PIPELINE_HPP
