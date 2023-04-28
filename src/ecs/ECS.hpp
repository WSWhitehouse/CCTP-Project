#ifndef SNOWFLAKE_ECS_HPP
#define SNOWFLAKE_ECS_HPP

#include "pch.hpp"

/**
* @file ECS.hpp
* @brief
*
* HOW TO USE:
* TODO(WSWhitehouse)
*
* USEFUL LINKS & RESOURCES:
*  - https://www.dataorienteddesign.com/dodbook/
*  - https://www.dataorienteddesign.com/dodmain/node14.html
*  - https://github.com/roig/destral_ecs
*  - https://gamesfromwithin.com/managing-data-relationships
*  - http://gameprogrammingpatterns.com/data-locality.html
*  - Building a Data-Oriented Entity System (4 part blog series):
*     - 1: https://bitsquid.blogspot.com/2014/08/building-data-oriented-entity-system.html
*     - 2: https://bitsquid.blogspot.com/2014/09/building-data-oriented-entity-system.html
*     - 3: https://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html
*     - 4: https://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system_10.html
*  - https://medium.com/ingeniouslysimple/entities-components-and-systems-89c31464240d
*  - https://github.com/skypjack/entt
*/

// ECS includes
#include "ecs/Entity.hpp"
#include "ecs/managers/Component.hpp"
#include "ecs/managers/SystemManager.hpp"

namespace ECS
{

  struct Manager
  {
    // --- ECS SYSTEM MANAGEMENT --- //
    void CreateECS();
    void DestroyECS();

    void ResetECS();

    // --- ENTITY MANAGEMENT --- //
    Entity CreateEntity();
    void DestroyEntity(Entity entity);

    // --- COMPONENT MANAGEMENT --- //
    template<typename T>
    T* AddComponent(Entity entity);

    template<typename T>
    void RemoveComponent(Entity entity);

    template<typename T> [[nodiscard]]
    b8 HasComponent(Entity entity) const;

    template<typename T> [[nodiscard]]
    T* GetComponent(Entity entity) const;

    template<typename T> [[nodiscard]]
    ComponentSparseSet* GetComponentSparseSet();

    template<typename T> [[nodiscard]]
    const ComponentSparseSet* GetComponentSparseSet() const;

    // --- SYSTEMS MANAGEMENT --- //
    void SystemsUpdate();

  private:
    ComponentSparseSet* components = nullptr;
    Entity* availableEntities      = nullptr;
    u32 availableEntityCount       = 0;
  };

} // namespace ECS

// --- TEMPLATE IMPLEMENTATION --- //

template<typename T>
T* ECS::Manager::AddComponent(ECS::Entity entity)
{
  ComponentSparseSet& sparseSet = components[Component<T>::INDEX];
  return sparseSet.AddComponent<T>(entity);
}

template<typename T>
void ECS::Manager::RemoveComponent(ECS::Entity entity)
{
  ComponentSparseSet& sparseSet = components[Component<T>::INDEX];
  return sparseSet.RemoveComponent<T>(entity);
}

template<typename T>
b8 ECS::Manager::HasComponent(ECS::Entity entity) const
{
  ComponentSparseSet& sparseSet = components[Component<T>::INDEX];
  return sparseSet.HasComponent<T>(entity);
}

template<typename T>
T* ECS::Manager::GetComponent(ECS::Entity entity) const
{
  ComponentSparseSet& sparseSet = components[Component<T>::INDEX];
  return sparseSet.GetComponent<T>(entity);
}

template<typename T>
ECS::ComponentSparseSet* ECS::Manager::GetComponentSparseSet()
{
  return &components[Component<T>::INDEX];
}

template<typename T>
const ECS::ComponentSparseSet* ECS::Manager::GetComponentSparseSet() const
{
  return &components[Component<T>::INDEX];
}

#endif //SNOWFLAKE_ECS_HPP
