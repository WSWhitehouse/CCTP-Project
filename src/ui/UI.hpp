#ifndef SNOWFLAKE_UI_HPP
#define SNOWFLAKE_UI_HPP

#include "pch.hpp"

// core
#include "core/Window.hpp"

// vulkan
#include "renderer/vk/Vulkan.hpp"

#define UI_IMAGE_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT

#define UI_TARGET_SCREEN_WIDTH  1920
#define UI_TARGET_SCREEN_HEIGHT 1080
#define UI_TARGET_SCREEN_SIZE   glm::vec2((f32)UI_TARGET_SCREEN_WIDTH, (f32)UI_TARGET_SCREEN_HEIGHT)

// Forward Declarations
namespace ECS { struct Manager; }

namespace UI
{

  void Init();
  void Shutdown();
  void RecreateSwapchain();

  void SetDirty();
  void DrawUI(ECS::Manager& ecs);

  b8 HasRedrawnThisFrame();
  void ResetHasRedrawnThisFrame();

  void BlitUI(VkCommandBuffer cmdBuffer);

  /**
  * @brief Returns the current scale of the UI based on the target screen size.
  * @see UI_TARGET_SCREEN_SIZE
  * @return UI Scale.
  */
  [[nodiscard]] INLINE glm::vec2 GetCurrentScale() noexcept
  {
    const glm::vec2 windowSize = glm::vec2((f32)Window::GetWidth(), (f32)Window::GetHeight());
    return windowSize / UI_TARGET_SCREEN_SIZE;
  }

} // namespace UI

#endif //SNOWFLAKE_UI_HPP
