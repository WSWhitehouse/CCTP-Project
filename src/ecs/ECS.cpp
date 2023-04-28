#include "ecs/ECS.hpp"

// core
#include "core/Abort.hpp"
#include "core/Logging.hpp"

// preprocessor
#include "preprocessor/HasFunction.hpp"

// ecs
#include "ecs/ComponentRegistry.inl"

// systems
#include "ecs/systems/TransformSystem.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/systems/FlyCamSystem.hpp"
#include "ecs/systems/SpriteSystem.hpp"

using namespace ECS;

struct InitComponentData
{
  u64 size; // Must be sizeof(ComponentData<T>)
  u32 count;
};

template <std::size_t... Is>
static consteval auto GetInitComponentDataImpl(std::index_sequence<Is...>)
{
  return FArray<InitComponentData, sizeof...(Is)>
    {
      InitComponentData
      {
        .size  = sizeof(ComponentData<typename IndexToComponent<Is>::Type>),
        .count = ECS::Component<typename IndexToComponent<Is>::Type>::MAX_COUNT,
      }
      ...
    };
}

static consteval FArray<InitComponentData, COMPONENT_COUNT> GetInitComponentData()
{
  return GetInitComponentDataImpl(std::make_index_sequence<COMPONENT_COUNT>());
}

void Manager::CreateECS()
{
  // Components...
  components = (ComponentSparseSet*)mem_alloc(sizeof(ComponentSparseSet) * COMPONENT_COUNT);
  static constexpr FArray<InitComponentData, COMPONENT_COUNT> componentInitData = GetInitComponentData();

  for (u32 i = 0; i < COMPONENT_COUNT; ++i)
  {
    ComponentSparseSet& sparseSet     = components[i];
    const InitComponentData& initData = componentInitData[i];

    sparseSet.entitySparseArray = (u32*)mem_alloc(sizeof(u32) * MAX_ENTITY_COUNT);
    sparseSet.componentArray    = mem_alloc(initData.size * initData.count);
    sparseSet.componentCount    = 0;
    sparseSet.componentStride   = initData.size;
  }

  // Entities...
  availableEntities    = (Entity*)mem_alloc(sizeof(Entity) * MAX_ENTITY_COUNT);
  availableEntityCount = MAX_ENTITY_COUNT;

  for (u32 i = 0; i < MAX_ENTITY_COUNT; ++i)
  {
    availableEntities[i] = i;
  }
}

void Manager::DestroyECS()
{
  for (u32 i = 0; i < COMPONENT_COUNT; ++i)
  {
    ComponentSparseSet& sparseSet = components[i];

    mem_free(sparseSet.entitySparseArray);
    mem_free(sparseSet.componentArray);

    sparseSet.entitySparseArray = nullptr;
    sparseSet.componentArray    = nullptr;
    sparseSet.componentCount    = 0;
  }

  mem_free(components);
  components = nullptr;

  mem_free(availableEntities);
  availableEntities    = nullptr;
  availableEntityCount = 0;
}

void Manager::ResetECS()
{
  for (u32 i = 0; i < COMPONENT_COUNT; ++i)
  {
    ComponentSparseSet& sparseSet = components[i];
    sparseSet.componentCount      = 0;
  }

  availableEntityCount = MAX_ENTITY_COUNT;
  for (u32 i = 0; i < MAX_ENTITY_COUNT; ++i)
  {
    availableEntities[i] = i;
  }
}

Entity Manager::CreateEntity()
{
  if (availableEntityCount <= 0)
  {
    LOG_FATAL("No more available entities!");
    ABORT(AbortCode::ABORT_CODE_ECS_FAILURE);
  }

  Entity entity = availableEntities[0];

  availableEntityCount--;
  availableEntities[0] = availableEntities[availableEntityCount];
  availableEntities[availableEntityCount] = NULL_ENTITY;

  return entity;
}

void Manager::DestroyEntity(Entity entity)
{
  // TODO(WSWhitehouse): Should remove all components from the entity that is being destroyed.
  
  availableEntities[availableEntityCount] = entity;
  availableEntityCount++;
}

void Manager::SystemsUpdate()
{
  // Player Systems
  FlyCamSystem::Update(*this);
  CameraSystem::Update(*this);

  // Transform
  TransformSystem::Update(*this);
  SpriteSystem::Update(*this);
}
