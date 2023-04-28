#ifndef SNOWFLAKE_INPUT_HPP
#define SNOWFLAKE_INPUT_HPP

#include "pch.hpp"
#include "Keycodes.hpp"

namespace Input
{

  void ClearState();
  void UpdateState();

  // --- INPUT PROCESSING --- //
  void ProcessKey(const Key& key, b8 isPressed);
  void ProcessMouseButton(const MouseButton& button, b8 isPressed);
  void ProcessMousePosition(i32 x, i32 y);
  void ProcessMouseScroll(i32 delta);

  // --- KEYS --- //
  // Key state on current frame
  b8 IsKeyDown(Key key);
  b8 IsKeyUp(Key key);

  // Key state on previous frame
  b8 WasKeyDown(Key key);
  b8 WasKeyUp(Key key);

  // Was key pressed/release this frame
  b8 KeyPressedThisFrame(Key key);
  b8 KeyReleasedThisFrame(Key key);

  b8 AnyKeyPressedThisFrame();

  // --- MOUSE BUTTON --- //
  // Mouse Button state on current frame
  b8 IsMouseButtonDown(MouseButton button);
  b8 IsMouseButtonUp(MouseButton button);

  // Mouse Button state on previous frame
  b8 WasMouseButtonDown(MouseButton button);
  b8 WasMouseButtonUp(MouseButton button);

  // Was mouse button pressed/release this frame
  b8 MouseButtonPressedThisFrame(MouseButton button);
  b8 MouseButtonReleasedThisFrame(MouseButton button);

  // --- MOUSE POSITION --- //
  void GetMousePosition(i32* x, i32* y);
  void GetPrevMousePosition(i32* x, i32* y);

  void SetMousePosition(i32 x, i32 y);

  // --- MOUSE SCROLL --- //
  i32 GetMouseScroll();
  i32 GetPrevMouseScroll();

} // namespace Input

#endif //SNOWFLAKE_INPUT_HPP
