#ifndef SNOWFLAKE_RENDERER_HPP
#define SNOWFLAKE_RENDERER_HPP

#include "pch.hpp"

// renderer
#include "renderer/vk/Vulkan.hpp"
#include "renderer/GraphicsPipeline.hpp"

// ECS
#include "ecs/ECS.hpp"

// Forward Declarations
struct Mesh;
struct MeshGeometry;
struct MeshRenderer;

namespace Renderer
{
  b8 Init();
  void Shutdown();

  void BeginFrame(ECS::Manager& ecs);
  void DrawFrame(ECS::Manager& ecs);
  void EndFrame();

  /** @brief Wait for the device (GPU) to stop processing all work. */
  void WaitForDeviceIdle();

  /** @brief Get the current frame number. Is thread safe. */
  u64 GetFrameNumber();

  void WaitForFrame(u32 frame);

  PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineConfig& config);
  void DestroyGraphicsPipeline(PipelineHandle pipelineHandle);

  const GraphicsPipeline& GetGraphicsPipeline(PipelineHandle pipelineHandle);

  // --- GETTERS --- //
  [[nodiscard]]       VkInstance       GetInstance() noexcept;
  [[nodiscard]] const vk::Device&      GetDevice() noexcept;
  [[nodiscard]] const vk::Swapchain&   GetSwapchain() noexcept;
  [[nodiscard]]       VkRenderPass     GetRenderpass() noexcept;
  [[nodiscard]]       VkDescriptorPool GetDescriptorPool() noexcept;
  [[nodiscard]] const vk::CommandPool& GetGraphicsCommandPool() noexcept;
  [[nodiscard]] const vk::CommandPool& GetComputeCommandPool() noexcept;

} // namespace Renderer

#endif //SNOWFLAKE_RENDERER_HPP
