#include "pch.hpp"

#if defined(PLATFORM_WINDOWS)

#include "core/Logging.hpp"
#include "core/Platform.hpp"
#include <windows.h>

struct PlatformState
{
  HINSTANCE hinstance;

  f64 clockFreq;
  LARGE_INTEGER startTime;
};

static PlatformState state = {};

b8 Platform::Init()
{
  state.hinstance = GetModuleHandleA(nullptr);

  // Set up time
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  state.clockFreq = 1.0 / (f64)freq.QuadPart;
  QueryPerformanceCounter(&state.startTime);

  return true;
}

void Platform::Shutdown()
{
  state.clockFreq = 0.0;
}

f64 Platform::GetTime()
{
  LARGE_INTEGER now_time;
  QueryPerformanceCounter(&now_time);
  return (f64)now_time.QuadPart * state.clockFreq;
}

void* Platform::GetNativeInstance()
{
  return &state.hinstance;
}

#endif
