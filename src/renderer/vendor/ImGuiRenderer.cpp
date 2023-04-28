#include "renderer/vendor/ImGuiRenderer.hpp"

#include "core/Logging.hpp"

#include "backends/imgui_impl_vulkan.h"

#include "renderer/Renderer.hpp"
#include "renderer/vk/Vulkan.hpp"

static void CheckVkResultFn(VkResult result)
{
  VK_SUCCESS_CHECK(result);
}

void ImGuiRenderer::Init()
{
  LOG_INFO("Initialising ImGui...");

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO& io = ImGui::GetIO();

  // Set up multiple viewports
  // TODO(WSWhitehouse): Fix renderpass validation error...
  //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Set up input
  // TODO(WSWhitehouse): Keyboard input conflicts with moving camera...
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Gamepad Controls

  // Set up fonts
  io.Fonts->AddFontDefault();

  PlatformInit();
  RendererInit();

  // Set default settings
  ImGui::SetColorEditOptions(ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);

  LOG_INFO("ImGui Initialised!");
}

void ImGuiRenderer::NewFrame()
{
  ImGui_ImplVulkan_NewFrame();
  PlatformNewFrame();
  ImGui::NewFrame();
}

void ImGuiRenderer::Render(VkCommandBuffer buffer)
{
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  ImGui_ImplVulkan_RenderDrawData(draw_data, buffer);

  // Update and Render additional platform windows
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

void ImGuiRenderer::RecreateSwapchain()
{
  // REVIEW(WSWhitehouse): This isn't a great way of recreating the backend for ImGui...
  Shutdown();
  Init();
}

void ImGuiRenderer::Shutdown()
{
  ImGui::EndFrame();
  RendererShutdown();
  PlatformShutdown();
  ImGui::DestroyContext();
}

static i32 CreateVkSurface(ImGuiViewport* vp, ImU64 vk_inst, const void* vk_allocators, ImU64* out_vk_surface)
{
  // REVIEW(WSWhitehouse): Casting the platform user data as under the hood its the window handle.
  if (!vk::CreateVkSurface((void*)vp->PlatformUserData, (VkInstance)vk_inst, (VkAllocationCallbacks*)vk_allocators, (VkSurfaceKHR*)out_vk_surface))
  {
    return VK_ERROR_UNKNOWN;
  }

  return VK_SUCCESS;
}

void ImGuiRenderer::RendererInit()
{
  ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
  platformIO.Platform_CreateVkSurface = CreateVkSurface;

  const vk::Device& device       = Renderer::GetDevice();
  const vk::Swapchain& swapchain = Renderer::GetSwapchain();

  // Setup Vulkan Renderer backend
  ImGui_ImplVulkan_InitInfo initInfo = {};
  initInfo.Instance        = Renderer::GetInstance();
  initInfo.PhysicalDevice  = device.physicalDevice;
  initInfo.Device          = device.logicalDevice;
  initInfo.QueueFamily     = device.queueFamilyIndices.graphicsFamily;
  initInfo.Queue           = device.graphicsQueue;
  initInfo.PipelineCache   = VK_NULL_HANDLE;
  initInfo.DescriptorPool  = Renderer::GetDescriptorPool();
  initInfo.Subpass         = 0;
  initInfo.MinImageCount   = swapchain.imagesCount;
  initInfo.ImageCount      = swapchain.imagesCount;
  initInfo.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
  initInfo.Allocator       = nullptr;
  initInfo.CheckVkResultFn = CheckVkResultFn;
  ImGui_ImplVulkan_Init(&initInfo, Renderer::GetRenderpass());

  // Create Requested Fonts
  // https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp
  {
    const vk::CommandPool& cmdPool = Renderer::GetGraphicsCommandPool();
    VkCommandBuffer cmdBuffer      = cmdPool.SingleTimeCommandBegin(device);

    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);

    cmdPool.SingleTimeCommandEnd(device, cmdBuffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
}

void ImGuiRenderer::RendererShutdown()
{
  ImGui_ImplVulkan_Shutdown();
}