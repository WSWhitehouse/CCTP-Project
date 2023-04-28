#include "pch.hpp"

#if defined(PLATFORM_WINDOWS)

#include "renderer/vendor/ImGuiRenderer.hpp"
#include "backends/imgui_impl_win32.h"
#include "core/Window.hpp"
#include "core/Assert.hpp"

void ImGuiRenderer::PlatformInit()
{
  HWND* hwnd = (HWND*)Window::GetNativeHandle();
  ASSERT_MSG(hwnd != nullptr, "Window Handle is null! Ensure Window is valid before initialising gui!");

  ImGui_ImplWin32_Init(*hwnd);
}

void ImGuiRenderer::PlatformNewFrame()
{
  ImGui_ImplWin32_NewFrame();
}

void ImGuiRenderer::PlatformShutdown()
{
  ImGui_ImplWin32_Shutdown();
}

#endif