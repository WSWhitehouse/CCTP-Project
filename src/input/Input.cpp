#include "pch.hpp"
#include "core/Logging.hpp"
#include "input/Input.hpp"

struct KeyboardState
{
  b8 keys[256];
};

struct MouseState
{
  i32 xPos;
  i32 yPos;
  i32 scrollDelta;
  b8 buttons[(u64)MouseButton::MAX_BUTTONS];
};

struct InputState
{
  KeyboardState currentKeyboardState;
  KeyboardState previousKeyboardState;

  MouseState currentMouseState;
  MouseState previousMouseState;

  b8 keyPressedThisFrame;
  b8 keyPressedPrevFrame;
};

static InputState state = {};

void Input::ClearState()
{
  mem_zero(&state, sizeof(InputState));
}

void Input::UpdateState()
{
  // Copy current state to prev state
  mem_copy(&state.previousKeyboardState, &state.currentKeyboardState, sizeof(KeyboardState));
  mem_copy(&state.previousMouseState, &state.currentMouseState, sizeof(MouseState));

  state.keyPressedThisFrame = false;
}

void Input::ProcessKey(const Key &key, b8 isPressed)
{
  u64 keyIndex = (u64)(key);

  // Return if the incoming value is the same as the current state
  if (state.currentKeyboardState.keys[keyIndex] == isPressed) return;

  // Update current state with new is_pressed value
  state.currentKeyboardState.keys[keyIndex] = isPressed;
  state.keyPressedThisFrame = true;
  // TODO(WSWhitehouse): Fire event
}

void Input::ProcessMouseButton(const MouseButton &button, b8 isPressed)
{
  u64 buttonIndex = (u64)(button);

  // Return if the incoming value is the same as the current state
  if (state.currentMouseState.buttons[buttonIndex] == isPressed) return;

  // Update current state with new is_pressed value
  state.currentMouseState.buttons[buttonIndex] = isPressed;
  // TODO(WSWhitehouse): Fire event
}

void Input::ProcessMousePosition(i32 x, i32 y)
{
  // Return if the incoming values are the same as the current state
  if (state.currentMouseState.xPos == x &&
      state.currentMouseState.yPos == y) return;

  // Update current state with new position values
  state.currentMouseState.xPos = x;
  state.currentMouseState.yPos = y;
  // TODO(WSWhitehouse): Fire event
}

void Input::ProcessMouseScroll(i32 delta)
{
  // Return if the incoming value is the same as the current state
  if (state.currentMouseState.scrollDelta == delta) return;

  // Update current state with new delta value
  state.currentMouseState.scrollDelta = delta;
  // TODO(WSWhitehouse): Fire event
}

b8 Input::IsKeyDown(Key key)
{
  u64 keyIndex = (u64)(key);
  return state.currentKeyboardState.keys[keyIndex] == true;
}

b8 Input::IsKeyUp(Key key)
{
  u64 keyIndex = (u64)(key);
  return state.currentKeyboardState.keys[keyIndex] == false;
}

b8 Input::WasKeyDown(Key key)
{
  u64 keyIndex = (u64)(key);
  return state.previousKeyboardState.keys[keyIndex] == true;
}

b8 Input::WasKeyUp(Key key)
{
  u64 keyIndex = (u64)(key);
  return state.previousKeyboardState.keys[keyIndex] == false;
}

b8 Input::KeyPressedThisFrame(Key key)
{
  return IsKeyDown(key) && WasKeyUp(key);
}

b8 Input::KeyReleasedThisFrame(Key key)
{
  return IsKeyUp(key) && WasKeyDown(key);
}

b8 Input::AnyKeyPressedThisFrame()
{
  return state.keyPressedThisFrame;
}

b8 Input::IsMouseButtonDown(MouseButton button)
{
  u64 buttonIndex = (u64)(button);
  return state.currentMouseState.buttons[buttonIndex] == true;
}

b8 Input::IsMouseButtonUp(MouseButton button)
{
  u64 buttonIndex = (u64)(button);
  return state.currentMouseState.buttons[buttonIndex] == false;
}

b8 Input::WasMouseButtonDown(MouseButton button)
{
  u64 buttonIndex = (u64)(button);
  return state.previousMouseState.buttons[buttonIndex] == true;
}

b8 Input::WasMouseButtonUp(MouseButton button)
{
  u64 buttonIndex = (u64)(button);
  return state.previousMouseState.buttons[buttonIndex] == false;
}

b8 Input::MouseButtonPressedThisFrame(MouseButton button)
{
  return IsMouseButtonDown(button) && WasMouseButtonUp(button);
}

b8 Input::MouseButtonReleasedThisFrame(MouseButton button)
{
  return IsMouseButtonUp(button) && WasMouseButtonDown(button);
}

void Input::GetMousePosition(i32 *x, i32 *y)
{
  *x = state.currentMouseState.xPos;
  *y = state.currentMouseState.yPos;
}

void Input::GetPrevMousePosition(i32 *x, i32 *y)
{
  *x = state.previousMouseState.xPos;
  *y = state.previousMouseState.yPos;
}

i32 Input::GetMouseScroll()
{
  return state.currentMouseState.scrollDelta;
}

i32 Input::GetPrevMouseScroll()
{
  return state.previousMouseState.scrollDelta;
}
