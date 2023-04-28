#include "ecs/systems/FlyCamSystem.hpp"

#include "ecs/ECS.hpp"
#include "ecs/components/FlyCam.hpp"
#include "ecs/components/Transform.hpp"

#include "core/AppTime.hpp"
#include "input/Input.hpp"
#include "core/Window.hpp"

using namespace ECS;

// Forward Declarations
static INLINE void UpdateRotation(FlyCam& flyCam, Transform* transform);
static INLINE void UpdatePosition(FlyCam& flyCam, Transform* transform);

void FlyCamSystem::Update(ECS::Manager& ecs)
{
  ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<FlyCam>();
  for (u32 i = 0; i < sparseSet->componentCount; ++i)
  {
    ComponentData<FlyCam>& componentData = ((ComponentData<FlyCam>*)sparseSet->componentArray)[i];
    FlyCam& flyCam = componentData.component;

    if (!ecs.HasComponent<Transform>(componentData.entity)) continue;
    Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    UpdateRotation(flyCam, transform);
    UpdatePosition(flyCam, transform);
  }
}

static INLINE void UpdateRotation(FlyCam& flyCam, Transform* transform)
{
  const MouseButton mouseButton = MouseButton::BUTTON_RIGHT;

  // Get window area
  const i32 width  = Window::GetWidth();
  const i32 height = Window::GetHeight();
  const glm::vec2 screenSize = glm::vec2((f32)width, (f32)height);

  if (!Input::IsMouseButtonDown(mouseButton)) return;

  // Get current mouse position
  i32 x, y;
  Input::GetMousePosition(&x, &y);

  b8 mouseWrapped = false;

  // NOTE(WSWhitehouse):
  // Clamping mouse position to the screen while moving the camera,
  // this ensures the cursor doesn't get stuck anywhere or leaves the
  // window area. Also updating the position so the movement doesn't
  // get messed up when wrapping the cursor.
  {
    if (x < 0)
    {
      Input::SetMousePosition(width, y);
      x = width;
      mouseWrapped = true;
    }

    if (x > width)
    {
      Input::SetMousePosition(0, y);
      x = 0;
      mouseWrapped = true;
    }

    if (y < 0)
    {
      Input::SetMousePosition(x, height);
      y = height;
      mouseWrapped = true;
    }

    if (y > height)
    {
      Input::SetMousePosition(x, 0);
      y = 0;
      mouseWrapped = true;
    }
  }

  glm::vec2 mousePos = {(f32)(y), (f32)(x)};

  if (!Input::WasMouseButtonDown(mouseButton) || mouseWrapped)
  {
    flyCam.prevMousePos = mousePos;
  }

  const glm::vec2 diff = flyCam.prevMousePos - mousePos;
  if (glm::length(diff) > 0.0001f)
  {
    const glm::vec2 rotation = (diff / screenSize) * glm::vec2(flyCam.lookSpeed.y, flyCam.lookSpeed.x);
    transform->rotation += glm::vec3(-rotation.x, -rotation.y, 0.0f);
  }

  // NOTE(WSWhitehouse): limit pitch values between about +/- 85ish degrees
  transform->rotation.x = CLAMP(transform->rotation.x, -1.5f, 1.5f);
  transform->rotation.y = MOD(transform->rotation.y, TAU);

  flyCam.prevMousePos = mousePos;
}

static INLINE void UpdatePosition(FlyCam& flyCam, Transform* transform)
{
  const f32 deltaTime = (f32)AppTime::DeltaTime();

  const glm::vec3 forwardDir = { glm::sin(transform->rotation.y), -transform->rotation.x, glm::cos(transform->rotation.y) };
  const glm::vec3 rightDir   = {forwardDir.z, 0.0f, -forwardDir.x};
  const glm::vec3 upDir      = {0.0f, -1.0f, 0.0f};

  glm::vec3 moveDir = {0.0f, 0.0f, 0.0f};
  if (Input::IsKeyDown(Key::W)) moveDir += forwardDir;
  if (Input::IsKeyDown(Key::S)) moveDir -= forwardDir;
  if (Input::IsKeyDown(Key::D)) moveDir += rightDir;
  if (Input::IsKeyDown(Key::A)) moveDir -= rightDir;

//  if (Input::IsKeyDown(Key::Q)) moveDir += upDir;
//  if (Input::IsKeyDown(Key::E)) moveDir -= upDir;

  // NOTE(WSWhitehouse): Taking a copy of move speed so it can be safely doubled when holding shift...
  f32 moveSpeed = flyCam.moveSpeed;
  if (Input::IsKeyDown(Key::SHIFT)) moveSpeed *= 2;

  if (glm::dot(moveDir, moveDir) > F32_EPSILON)
  {
    transform->position += moveSpeed * deltaTime * glm::normalize(moveDir);
  }
}