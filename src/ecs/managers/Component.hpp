#ifndef SNOWFLAKE_COMPONENT_HPP
#define SNOWFLAKE_COMPONENT_HPP

#include "pch.hpp"

#include <new> // placement new

// core
#include "core/Logging.hpp"

// ECS includes
#include "ecs/Entity.hpp"

// NOTE(WSWhitehouse): The clang compiler warns against undefined var templates, but this
// isn't a problem as they are just defined in another translation unit.
#if defined(COMPILER_CLANG)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

namespace ECS
{

  /**
  * @brief The constant static data associated with a component.
  * All components must be registered (see ComponentRegistry.inl).
  * @tparam T Component Type.
  */
  template<typename T>
  struct Component
  {
    static const char* NAME;
    static const u64 UUID;

    static const u64 INDEX;
    static const u32 MAX_COUNT;
  };

  /**
  * @brief The component data stored in the Sparse Set.
  * @tparam T Component Type.
  */
  template<typename T>
  struct ComponentData
  {
    Entity entity = NULL_ENTITY;
    T component   = {};
  };

  /**
  * @brief The sparse set used to manage components and entity
  * relationships. This is used in the ECS::Manager to hold
  * components.
  */
  struct ComponentSparseSet
  {
    u32* entitySparseArray = nullptr;
    void* componentArray   = nullptr;
    u32 componentCount     = 0;
    u32 componentStride    = 0;

    /**
    * @brief Add component to entity
    * @param entity Entity to add component too.
    * @return Pointer to newly added component.
    */
    template<typename T>
    INLINE T* AddComponent(Entity entity)
    {
      if (componentCount >= Component<T>::MAX_COUNT)
      {
        LOG_ERROR("Max component count hit on '%s' Component!", Component<T>::NAME);
        return nullptr;
      }

      if (HasComponent<T>(entity))
      {
        LOG_ERROR("Trying to add %s component to entity which already has this component!", Component<T>::NAME);
        return nullptr;
      }

      ComponentData<T>* compArray = (ComponentData<T>*)componentArray;

      compArray[componentCount].entity = entity;
      mem_zero(&compArray[componentCount].component, sizeof(T));

      // NOTE(WSWhitehouse): This is "placement new" which doesn't allocate any
      // memory but calls the constructor on already allocated memory. This
      // allows values to be set to their default, etc.
      new (&compArray[componentCount].component) T();

      entitySparseArray[entity] = componentCount;

      componentCount++;

      return GetComponent<T>(entity);
    }

    /**
    * @brief Remove a component from the entity.
    * @param entity Entity to remove component from.
    */
    template<typename T>
    INLINE void RemoveComponent(Entity entity)
    {
      if (!HasComponent<T>(entity)) return;

      // NOTE(WSWhitehouse): Perform any clean up by calling the components
      // destructor, does not actually delete the component or clear its memory.
      T* component = GetComponent<T>(entity);
      component->~T();

      componentCount--;

      // Move the last item in the dense component array into the empty
      // slot where the item we want to remove is...
      const u32 lastComponentIndex = componentCount;
      const u32 componentIndex     = entitySparseArray[entity];

      // Copy the data to the new index, using mem_copy to avoid copy ctors, etc.
      ComponentData<T>* compArray = (ComponentData<T>*)componentArray;
      mem_copy(&compArray[componentIndex],
               &compArray[lastComponentIndex],
               sizeof(ComponentData<T>));

      entitySparseArray[compArray[componentIndex].entity] = componentIndex;
    }

    /**
    * @brief Check if an entity has this component.
    * @param entity Entity to check.
    * @return True if the entity has this component; false otherwise.
    */
    template<typename T>
    [[nodiscard]] INLINE b8 HasComponent(Entity entity) const
    {
      const u32& componentIndex = entitySparseArray[entity];
      if (componentIndex >= componentCount) return false;

      ComponentData<T>* compArray = (ComponentData<T>*)componentArray;
      return compArray[componentIndex].entity == entity;
    }

    /**
    * @brief Gets the component associated with the entity. Does not
    * perform any safety checks on entity. Do NOT store returned pointer
    * for a long duration (i.e. between frames), the component for this
    * entity is not guaranteed to stay at the same location in memory.
    * @param entity Entity to get the component from.
    * @return Pointer to component.
    */
    template<typename T>
    [[nodiscard]] INLINE T* GetComponent(Entity entity) const
    {
      const u32& componentIndex = entitySparseArray[entity];
      ComponentData<T>* compArray = (ComponentData<T>*)componentArray;
      return &compArray[componentIndex].component;
    }

  };

} // namespace ECS

#if defined(COMPILER_CLANG)
  #pragma clang diagnostic pop
#endif

#endif //SNOWFLAKE_COMPONENT_HPP
