#include "pch.hpp"

#if defined(PLATFORM_WINDOWS)

#include "core/Logging.hpp"
#include "input/Input.hpp"
#include "core/Window.hpp"
#include <windows.h>

void Input::SetMousePosition(i32 x, i32 y)
{
  POINT point = {x, y};
  ClientToScreen(*((HWND*)Window::GetNativeHandle()), &point);
  SetCursorPos(point.x, point.y);

  // NOTE(WSWhitehouse): Update internal input data...
  Input::ProcessMousePosition(x, y);
}

#endif // PLATFORM_WINDOWS