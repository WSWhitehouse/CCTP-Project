#ifndef SNOWFLAKE_IMGUIRENDERER_HPP
#define SNOWFLAKE_IMGUIRENDERER_HPP

#include "pch.hpp"
#include "imgui.h"
#include <vulkan/vulkan.h>

// NOTE(WSWhitehouse):
// This class is a wrapper on the ImGui code, it handles initialising the backends with
// the Vulkan renderer and specific platform setup.
class ImGuiRenderer
{
  DECLARE_STATIC_CLASS(ImGuiRenderer);

public:
  static void Init();
  static void NewFrame();
  static void Render(VkCommandBuffer buffer);
  static void RecreateSwapchain();
  static void Shutdown();

private:
  // Platform Backend Functions
  static void PlatformInit();
  static void PlatformNewFrame();
  static void PlatformShutdown();

  // Renderer Backend Functions
  static void RendererInit();
  static void RendererShutdown();
};

#endif //SNOWFLAKE_IMGUIRENDERER_HPP
