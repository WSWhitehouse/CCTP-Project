#ifndef SNOWFLAKE_WINDOW_HPP
#define SNOWFLAKE_WINDOW_HPP

#include "pch.hpp"

namespace Window
{

  b8 Create(const std::string& name, i32 width, i32 height);
  void Destroy();

  void SetTitle(const char* name);

  // TODO(WSWhitehouse): Move this to an "engine event system" (yet to be created).
  void SetOnWindowResizedCallback(void(*callback)(int width, int height));

  /** @brief Handles the window message queue, returns if there are no messages */
  void HandleMessages();

  /** @brief Handles the window message queue, waits for a message. */
  void WaitMessages();

  /**
   * Get the window handle for the native platform.
   * - Win32 returns HWND
   * @return Native window handle
   * */
  [[nodiscard]] void* GetNativeHandle();

  // --- GETTERS --- //
  [[nodiscard]] const i32& GetWidth();
  [[nodiscard]] const i32& GetHeight();
  [[nodiscard]] b8 GetIsFocussed();
  [[nodiscard]] INLINE f32 GetAspectRatio() { return (f32)GetWidth() / (f32)GetHeight(); }
  [[nodiscard]] const i32& GetRefreshRate();

} // namespace Window

#endif //SNOWFLAKE_WINDOW_HPP
