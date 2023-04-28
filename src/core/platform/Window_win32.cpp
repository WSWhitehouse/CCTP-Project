#include "pch.hpp"

#if defined(PLATFORM_WINDOWS)

// core
#include "core/Logging.hpp"
#include "core/Platform.hpp"
#include "core/Window.hpp"

// application
#include "application/Application.hpp"

// input
#include "input/Input.hpp"

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

// Used to process key and mouse events for imgui
#include "backends/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward Declarations
LRESULT CALLBACK ProcessMessage(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);
static INLINE void UpdateRefreshRate();

struct WindowState
{
  // Handles
  HWND hwnd;
  HMONITOR primaryMonitor;

  // Window Data
  b8 isFocussed;
  i32 windowWidth;
  i32 windowHeight;
  i32 refreshRate;

  // Callbacks
  void(*windowResizeCallback)(int width, int height);
};

static WindowState state = {};

b8 Window::Create(const std::string& name, i32 width, i32 height)
{
  HINSTANCE hinstance = *(HINSTANCE*)Platform::GetNativeInstance();

  // Set up window icon
  HICON icon = LoadIcon(hinstance, IDI_APPLICATION);

  std::string className = name + "_class";

  // Register window class
  WNDCLASS wndClass;
  mem_set(&wndClass, 0, sizeof(WNDCLASS));
  wndClass.style         = CS_DBLCLKS; // Get double clicks
  wndClass.lpfnWndProc   = ProcessMessage;
  wndClass.cbClsExtra    = 0;
  wndClass.cbWndExtra    = 0;
  wndClass.hInstance     = hinstance;
  wndClass.hIcon         = icon;
  wndClass.hCursor       = LoadCursor(nullptr, IDC_ARROW); // NULL = manage cursor manually
  wndClass.hbrBackground = nullptr; // Transparent
  wndClass.lpszClassName = className.c_str();

  if (!RegisterClass(&wndClass))
  {
    LOG_FATAL("Window class registration failed!");
    return false;
  }

  // Set up window style
  u32 windowStyle   = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
  u32 windowExStyle = WS_EX_APPWINDOW;

  // Get the primary monitor handle
  {
    const POINT zero     = {0, 0};
    state.primaryMonitor = MonitorFromPoint(zero, MONITOR_DEFAULTTOPRIMARY);
  }

  i32 windowXPos     = 0;
  i32 windowYPos     = 0;
  state.windowWidth  = width;
  state.windowHeight = height;

  // Calculate the size of the primary monitor and place the window in the center...
  MONITORINFOEX monitorInfo = {};
  monitorInfo.cbSize        = sizeof(MONITORINFOEX);
  if (GetMonitorInfo(state.primaryMonitor, &monitorInfo))
  {
    DEVMODE devmode       = {};
    devmode.dmSize        = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode))
    {
      u32 monitorX = devmode.dmPelsWidth;
      u32 monitorY = devmode.dmPelsHeight;

      // NOTE(WSWhitehouse): If the requested window is larger than the monitor, set the
      // window size to match. This ensures it always fits to the screen nicely.
      if ((i32)monitorX < state.windowWidth)  state.windowWidth  = (i32)monitorX;
      if ((i32)monitorY < state.windowHeight) state.windowHeight = (i32)monitorY;

      f32 halfMonitorX = ((f32)monitorX) * 0.5f;
      f32 halfMonitorY = ((f32)monitorY) * 0.5f;
      f32 halfWindowX  = ((f32)state.windowWidth)  * 0.5f;
      f32 halfWindowY  = ((f32)state.windowHeight) * 0.5f;

      windowXPos = (i32)(halfMonitorX - halfWindowX);
      windowYPos = (i32)(halfMonitorY - halfWindowY);
    }
  }

  // NOTE(WSWhitehouse): The window size accounts for the entire window including the border and title bar.
  // This adjustment rect gets the values that are to be added to the window size to create a client window
  // area that is the same size as requested.
  RECT adjustmentRect = {0, 0, 0, 0};
  AdjustWindowRectEx(&adjustmentRect, windowStyle, 0, windowExStyle);

  // Updating window size
  windowXPos += adjustmentRect.left;
  windowYPos += adjustmentRect.top;
  state.windowWidth  += adjustmentRect.right  - adjustmentRect.left;
  state.windowHeight += adjustmentRect.bottom - adjustmentRect.top;

  // Create window
  HWND hwnd = CreateWindowEx(windowExStyle, className.c_str(), name.c_str(),
                             windowStyle, windowXPos, windowYPos, state.windowWidth, state.windowHeight,
                             nullptr, nullptr, hinstance, nullptr);

  if (hwnd == nullptr)
  {
    LOG_FATAL("Window creation failed!");
    return false;
  }

  state.hwnd = hwnd;
  UpdateRefreshRate();

  // Activate window
  constexpr const b32 shouldActivate     = true;
  constexpr const i32 showWindowCmdFlags = shouldActivate ? SW_SHOW : SW_SHOWNOACTIVATE;
  ShowWindow(state.hwnd, showWindowCmdFlags);

  Input::ClearState();
  state.isFocussed = true;
  return true;
}

void Window::Destroy()
{
  if (state.hwnd == nullptr) return;

  DestroyWindow(state.hwnd);
  state.hwnd        = nullptr;
}

void Window::SetTitle(const char* name)
{
  SetWindowText(state.hwnd, name);
}

void Window::SetOnWindowResizedCallback(void (*callback)(int, int))
{
  state.windowResizeCallback = callback;
}

void Window::HandleMessages()
{
  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
}

void Window::WaitMessages()
{
  MSG msg;
  GetMessageA(&msg, nullptr, 0, 0);
  TranslateMessage(&msg);
  DispatchMessageA(&msg);
}

void* Window::GetNativeHandle()     { return (void*)&state.hwnd; }
const i32& Window::GetWidth()       { return state.windowWidth;  }
const i32& Window::GetHeight()      { return state.windowHeight; }
b8 Window::GetIsFocussed()          { return state.isFocussed;   }
const i32& Window::GetRefreshRate() { return state.refreshRate;  }

static INLINE void UpdateRefreshRate()
{
  constexpr const i32 DefaultRefreshRate = 60;

  HMONITOR monitor = MonitorFromWindow(state.hwnd, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFOEX monitorInfo = {};
  monitorInfo.cbSize        = sizeof(MONITORINFOEX);
  if (!GetMonitorInfo(monitor, &monitorInfo))
  {
    state.refreshRate = DefaultRefreshRate;
    return;
  }

  DEVMODE devmode       = {};
  devmode.dmSize        = sizeof(DEVMODE);
  devmode.dmDriverExtra = 0;
  if (!EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode))
  {
    state.refreshRate = DefaultRefreshRate;
    return;
  }

  state.refreshRate = (i32)devmode.dmDisplayFrequency;
}

LRESULT CALLBACK ProcessMessage(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param)
{
  switch (msg)
  {
    case WM_ERASEBKGND: return 1; // Notify the OS that erasing will be handled by the application to prevent flicker

    case WM_SETFOCUS:
    {
      state.isFocussed = true;
    } return 0;

    case WM_KILLFOCUS:
    {
      state.isFocussed = false;
    } return 0;

    case WM_CLOSE:
    {
      Application::Quit();
    } return 0;

    case WM_DESTROY:
    {
      PostQuitMessage(0);
    } return 0;

    case WM_SIZE:
    {
      // Get the updated size
      RECT r;
      GetClientRect(hwnd, &r);
      state.windowWidth  = r.right - r.left;
      state.windowHeight = r.bottom - r.top;

      UpdateRefreshRate();

      // Fire the window resize callback if its valid
      if (state.windowResizeCallback != nullptr)
      {
        state.windowResizeCallback(state.windowWidth, state.windowHeight);
      }
    } break;

    case WM_MOVE:
    {
      UpdateRefreshRate();
      break;
    }

    // Key pressed/released
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
      b8 is_pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
      Key key       = (Key)((u64) w_param);

      // NOTE(WSWhitehouse): The Windows message system doesn't register the difference
      // between the left or right key for Menu(Alt), Shift, and Control in the w_param.
      // We need to manually check for the correct key below:
//      switch ((u64)w_param)
//      {
//        case VK_MENU: // Alt Key
//        {
//          if(GetKeyState(VK_RMENU) & 0x8000) { key = Key::RALT; }
//          if(GetKeyState(VK_LMENU) & 0x8000) { key = Key::LALT; }
//          break;
//        }
//
//        case VK_SHIFT:
//        {
//          if(GetKeyState(VK_RSHIFT) & 0x8000) { key = Key::RSHIFT; }
//          if(GetKeyState(VK_LSHIFT) & 0x8000) { key = Key::LSHIFT; }
//          break;
//        }
//
//        case VK_CONTROL:
//        {
//          if(GetKeyState(VK_RCONTROL) & 0x8000) { key = Key::RCONTROL; }
//          if(GetKeyState(VK_LCONTROL) & 0x8000) { key = Key::LCONTROL; }
//          break;
//        }
//
//        default: break;
//      }

      Input::ProcessKey(key, is_pressed);
    } break;

    // Mouse position
    case WM_MOUSEMOVE:
    {
      i32 x_pos = GET_X_LPARAM(l_param);
      i32 y_pos = GET_Y_LPARAM(l_param);
      Input::ProcessMousePosition(x_pos, y_pos);
    } break;

    // Mouse Scroll Wheel
    case WM_MOUSEWHEEL:
    {
      i32 scroll_delta = GET_WHEEL_DELTA_WPARAM(w_param);
      // Flatten the input to an OS-independent (-1, 1)
      if (scroll_delta != 0) scroll_delta = (scroll_delta < 0) ? -1 : 1;

      Input::ProcessMouseScroll(scroll_delta);
    } break;

    // Mouse Buttons
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
      b8 is_pressed = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN);

      // Get mouse button
      MouseButton mouse_button = MouseButton::MAX_BUTTONS;
      switch (msg)
      {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
          mouse_button = MouseButton::BUTTON_LEFT;
        } break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
          mouse_button = MouseButton::BUTTON_MIDDLE;
        } break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        {
          mouse_button = MouseButton::BUTTON_RIGHT;
        } break;
      }

      Input::ProcessMouseButton(mouse_button, is_pressed);
    } break;

    default: break;
  }

  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, w_param, l_param)) return true;

  return DefWindowProc(hwnd, msg, w_param, l_param);
}

#endif