#include "world/WorldManager.hpp"
#include "world/WorldRegistry.hpp"

// core
#include "core/Logging.hpp"

// ecs
#include "ecs/ECS.hpp"

// renderer
#include "renderer/Renderer.hpp"

// ui
#include "ui/UI.hpp"

static WorldID activeWorldID = 0;
static World activeWorld     = WORLD_REGISTRY[activeWorldID];

// NOTE(WSWhitehouse): If this doesn't match the currently active
// world ID then a LoadWorld function call has occurred.
static WorldID nextWorldID = 0;

static ECS::Manager ecs = {};

void WorldManager::Init()
{
  LOG_INFO("Initialising World...");
  ecs.CreateECS();
  UI::SetDirty();

  activeWorld.initFunc(ecs);
  LOG_INFO("World Initialised!");
}

void WorldManager::Shutdown()
{
  activeWorld.shutdownFunc(ecs);
  ecs.DestroyECS();
}

b8 WorldManager::LoadWorld(WorldID worldId)
{
  if (worldId >= WORLD_COUNT)
  {
    LOG_ERROR("Failed to load world at id: %u.", worldId);
    return false;
  }

  nextWorldID = worldId;
  return true;
}

void WorldManager::BeginFrame()
{
  // NOTE(WSWhitehouse): Checking if the world should change here...
  if (activeWorldID != nextWorldID)
  {
    // Must wait for all work to be completed from the previous frame before updating world...
    Renderer::WaitForDeviceIdle();

    activeWorld.shutdownFunc(ecs);
    ecs.ResetECS();

    activeWorldID = nextWorldID;
    activeWorld   = WORLD_REGISTRY[activeWorldID];
    activeWorld.initFunc(ecs);

    UI::SetDirty();
  }

  Renderer::BeginFrame(ecs);
}

void WorldManager::UpdateWorld()
{
  activeWorld.updateFunc(ecs);
  ecs.SystemsUpdate();

  Renderer::DrawFrame(ecs);
}

void WorldManager::EndFrame()
{
  Renderer::EndFrame();
}