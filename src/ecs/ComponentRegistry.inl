#ifndef SNOWFLAKE_COMPONENT_REGISTRY_HPP
#define SNOWFLAKE_COMPONENT_REGISTRY_HPP

#include "pch.hpp"

#include "core/Hash.hpp"
#include "preprocessor/Counter.hpp"

/**
* @brief Used when initialising the ECS::Manager to associate
* an ID with the component type. This is automatically managed
* through the ECS_REGISTER_COMPONENT macro.
* @tparam Index Index of component.
*/
template<u64 Index>
struct IndexToComponent
{
  using Type = void;
};

#define ECS_REGISTER_COMPONENT_BEGIN() COUNTER_NEW(comp_counter)
#define ECS_REGISTER_COMPONENT_COUNT() COUNTER_READ(comp_counter)

#define ECS_REGISTER_COMPONENT(type, maxComponentCount)                                    \
    template<> const char* ECS::Component<type>::NAME      = #type;                        \
    template<> const u64   ECS::Component<type>::UUID      = Hash::FNV1a64Str(#type);      \
    template<> const u64   ECS::Component<type>::INDEX     = COUNTER_READ(comp_counter);   \
    template<> const u32   ECS::Component<type>::MAX_COUNT = maxComponentCount;            \
    template<> struct IndexToComponent<COUNTER_READ(comp_counter)> { using Type = type; }; \
    COUNTER_INCREMENT(comp_counter)

// Components
#include "ecs/components/Transform.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/FlyCam.hpp"
#include "ecs/components/Skybox.hpp"
#include "ecs/components/PointLight.hpp"
#include "ecs/components/Sprite.hpp"
#include "ecs/components/SdfRenderer.hpp"
#include "ecs/components/SdfVoxelGrid.hpp"
#include "ecs/components/PointCloudRenderer.hpp"
#include "ecs/components/SdfPointCloudRenderer.hpp"
#include "ecs/components/UIImage.hpp"

// --- COMPONENT REGISTRY --- //
ECS_REGISTER_COMPONENT_BEGIN();

// --------------------- TYPE ----------- MAX COMPONENT COUNT --- //
ECS_REGISTER_COMPONENT(Transform,             ECS::MAX_ENTITY_COUNT);
ECS_REGISTER_COMPONENT(MeshRenderer,          ECS::MAX_ENTITY_COUNT);
ECS_REGISTER_COMPONENT(Sprite,                ECS::MAX_ENTITY_COUNT);
ECS_REGISTER_COMPONENT(Camera,                1                    );
ECS_REGISTER_COMPONENT(FlyCam,                1                    );
ECS_REGISTER_COMPONENT(Skybox,                1                    );
ECS_REGISTER_COMPONENT(PointLight,            MAX_POINT_LIGHT_COUNT);
ECS_REGISTER_COMPONENT(SdfRenderer,           5                    );
ECS_REGISTER_COMPONENT(SdfVoxelGrid,          5                    );
ECS_REGISTER_COMPONENT(PointCloudRenderer,    ECS::MAX_ENTITY_COUNT);
ECS_REGISTER_COMPONENT(SdfPointCloudRenderer, ECS::MAX_ENTITY_COUNT);
ECS_REGISTER_COMPONENT(UIImage,               ECS::MAX_ENTITY_COUNT);

static constexpr const u32 COMPONENT_COUNT = ECS_REGISTER_COMPONENT_COUNT();

// NOTE(WSWhitehouse): Undefining the register component
// macros so other scripts can't register components...
#undef ECS_REGISTER_COMPONENT_BEGIN
#undef ECS_REGISTER_COMPONENT_COUNT
#undef ECS_REGISTER_COMPONENT

#endif //SNOWFLAKE_COMPONENT_REGISTRY_HPP