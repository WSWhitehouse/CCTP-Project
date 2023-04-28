#include "ecs/systems/CameraSystem.hpp"

// core
#include "core/Window.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Transform.hpp"

using namespace ECS;

// Forward Declarations
static INLINE void UpdateCamera(Camera& camera, const Transform& transform);
static INLINE glm::mat4 CreateProjMatrix(f32 fovY, f32 aspect, f32 nearClipPlane, f32 farClipPlane);
static INLINE glm::mat4 CreateViewMatrixYXZ(glm::vec3 position, glm::vec3 rotation);

void CameraSystem::Update(ECS::Manager& ecs)
{
  ComponentSparseSet* sparseSet = ecs.GetComponentSparseSet<Camera>();
  for (u32 i = 0; i < sparseSet->componentCount; ++i)
  {
    ComponentData<Camera>& componentData = ((ComponentData<Camera>*)sparseSet->componentArray)[i];
    Camera& camera = componentData.component;

    if (!ecs.HasComponent<Transform>(componentData.entity)) continue;
    const Transform* transform = ecs.GetComponent<Transform>(componentData.entity);

    UpdateCamera(camera, *transform);
  }
}

static INLINE void UpdateCamera(Camera& camera, const Transform& transform)
{
  // Projection Matrix
  // TODO: Only update this when the window size changes...
  camera.projMatrix        = CreateProjMatrix(camera.fovY, Window::GetAspectRatio(), camera.nearClipPlane, camera.farClipPlane);
  camera.inverseProjMatrix = glm::inverse(camera.projMatrix);

  // View Matrix
  camera.viewMatrix        = CreateViewMatrixYXZ(transform.position, transform.rotation);
  camera.inverseViewMatrix = glm::inverse(camera.viewMatrix);
}

static INLINE glm::mat4 CreateProjMatrix(f32 fovY, f32 aspect, f32 nearClipPlane, f32 farClipPlane)
{
  // https://www.youtube.com/watch?v=rvJHkYnAR3w

  const float tanHalfFovY = tanf(fovY * 0.5f);

  glm::mat4 projMatrix = glm::mat4{0.0f};
  projMatrix[0][0] = 1.f / (aspect * tanHalfFovY);
  projMatrix[1][1] = (1.f / (tanHalfFovY)) * -1;
  projMatrix[2][2] = farClipPlane / (farClipPlane - nearClipPlane);
  projMatrix[2][3] = 1.f;
  projMatrix[3][2] = -(farClipPlane * nearClipPlane) / (farClipPlane - nearClipPlane);

  return projMatrix;
}

static INLINE glm::mat4 CreateViewMatrixYXZ(glm::vec3 position, glm::vec3 rotation)
{
  // https://www.youtube.com/watch?v=rvJHkYnAR3w

  const float c3 = glm::cos(rotation.z);
  const float s3 = glm::sin(rotation.z);
  const float c2 = glm::cos(rotation.x);
  const float s2 = glm::sin(rotation.x);
  const float c1 = glm::cos(rotation.y);
  const float s1 = glm::sin(rotation.y);

  const glm::vec3 u {(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
  const glm::vec3 v {(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
  const glm::vec3 w {(c2 * s1), (-s2), (c1 * c2)};

  glm::mat4 viewMatrix = glm::mat4{1.f};
  viewMatrix[0][0] = u.x;
  viewMatrix[1][0] = u.y;
  viewMatrix[2][0] = u.z;
  viewMatrix[0][1] = v.x;
  viewMatrix[1][1] = v.y;
  viewMatrix[2][1] = v.z;
  viewMatrix[0][2] = w.x;
  viewMatrix[1][2] = w.y;
  viewMatrix[2][2] = w.z;
  viewMatrix[3][0] = -glm::dot(u, position);
  viewMatrix[3][1] = -glm::dot(v, position);
  viewMatrix[3][2] = -glm::dot(w, position);

  return viewMatrix;
}
