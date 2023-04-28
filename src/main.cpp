#include "pch.hpp"

// core
#include "application/Application.hpp"
#include "application/ApplicationInfo.hpp"
#include "core/Logging.hpp"
#include "core/Profiler.hpp"
#include "core/Platform.hpp"
#include "core/Window.hpp"
#include "core/AppTime.hpp"

#include "input/Input.hpp"

// Job System
#include "threading/JobSystem.hpp"

// renderer
#include "renderer/Renderer.hpp"
#include "renderer/Gizmos.hpp"

// world
#include "world/WorldManager.hpp"

// asset database
#include "filesystem/AssetDatabase.hpp"

using namespace ECS;

int main()
{
  if (!Logging::Init())       return EXIT_FAILURE;
  if (!Platform::Init())      return EXIT_FAILURE;
  if (!JobSystem::Init())     return EXIT_FAILURE;
  if (!AssetDatabase::Init()) return EXIT_FAILURE;

  if (!Window::Create(Application::Name, 1920, 1080)) return EXIT_FAILURE;
//  if (!Window::Create(Application::Name, 1280, 720)) return EXIT_FAILURE;
  if (!Renderer::Init()) return EXIT_FAILURE;

  WorldManager::Init();

  char windowTitle[150];
  AppTime::Start();

  while(!Application::HasRequestedQuit())
  {
    // Start of frame...
    AppTime::Update();
    Window::HandleMessages();
    WorldManager::BeginFrame();

    // Update window title with dt and fps info...
    sprintf(windowTitle, "%s (dt: %f) (fps: %i)", Application::Name, AppTime::DeltaTime(), (i32)(1.0 / AppTime::DeltaTime()));
    Window::SetTitle(windowTitle);

    if (Input::KeyPressedThisFrame(Key::F3))
    {
      Gizmos::ToggleGizmos();
    }

    WorldManager::UpdateWorld();

    // End of frame...
    WorldManager::EndFrame();
    Input::UpdateState();
  }

  Renderer::WaitForDeviceIdle();

  WorldManager::Shutdown();

  Renderer::Shutdown();
  Window::Destroy();

  AssetDatabase::Shutdown();
  JobSystem::Shutdown();
  Platform::Shutdown();
  Logging::Shutdown();

  return EXIT_SUCCESS;
}
