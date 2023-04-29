#include "world/worlds/DissWorld.hpp"

// core
#include "core/Profiler.hpp"
#include "core/AppTime.hpp"

// containers
#include "containers/DArray.hpp"

// asset database
#include "filesystem/AssetDatabase.hpp"

// geometry
#include "geometry/Mesh.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/BoundingBox3D.hpp"
#include "geometry/ConvexHull.hpp"
#include "geometry/PointCloud.hpp"

// ecs
#include "ecs/ECS.hpp"
#include "ecs/ComponentFactory.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Skybox.hpp"
#include "ecs/components/FlyCam.hpp"
#include "ecs/components/SdfVoxelGrid.hpp"
#include "ecs/components/SdfRenderer.hpp"
#include "ecs/components/PointCloudRenderer.hpp"
#include "ecs/components/SdfPointCloudRenderer.hpp"
#include "ecs/components/PointLight.hpp"

// sdf
#include "containers/BSPTree.hpp"

// renderer
#include "renderer/Gizmos.hpp"

using namespace ECS;

struct TreeNode
{
  Entity entity;
  b8 isLeaf = false;
};

// entities
static Entity camera;
static Entity pointLight;
//static DArray<TreeNode> meshEntities = {};
//static Entity sdfVoxelGrid;
static Entity sdfRenderer;
//static Entity planeEntities = {};

void DissWorld::Init(ECS::Manager& ecs)
{
  // Create camera entity...
  camera = ecs.CreateEntity();
  {
    Transform* transform = ecs.AddComponent<Transform>(camera);
    transform->position = glm::vec3(0.0f, 0.0f, 0.0f);

    ecs.AddComponent<Camera>(camera);
    ecs.AddComponent<FlyCam>(camera);
  }

  // TODO(WSWhitehouse): Clean up mesh... its currently a mem leak.
//  Mesh* testMesh = AssetDatabase::LoadMesh("data/bunny.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/test-obj.glb");
  const Mesh* testMesh = AssetDatabase::LoadMesh("data/low-poly-sphere.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/sphere.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/stanford-bunny-high-res.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/sponza/sponza.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/monkey.glb");
  if (testMesh == nullptr) ABORT(ABORT_CODE_ASSET_FAILURE);

  LOG_DEBUG("Vertex Count: %u, Index Count: %u, Triangle Count: %u",
            testMesh->geometryArray[0].vertexCount, testMesh->geometryArray[0].indexCount, testMesh->geometryArray[0].indexCount / 3);

  sdfRenderer = ecs.CreateEntity();
  {
    Transform* transform = ecs.AddComponent<Transform>(sdfRenderer);
    transform->position  = {0.0f, 0.0f, 0.0f};
    transform->scale     = {1.0f, 1.0f, 1.0f};

    SdfRenderer* renderer = ecs.AddComponent<SdfRenderer>(sdfRenderer);

    {
      PROFILE_FUNC
      ComponentFactory::SdfRendererCreate(ecs, renderer, testMesh);
    }
  }

  pointLight = ecs.CreateEntity();
  {
    Transform* transform = ecs.AddComponent<Transform>(pointLight);
    transform->position = { 2.0f, 0.0f, 3.0f };

    PointLight* light = ecs.AddComponent<PointLight>(pointLight);
    light->range  = 5.0f;
    light->colour = {1.0f, 0.0f, 0.8f};
  }
}

void DissWorld::Shutdown(ECS::Manager& ecs)
{
}

void DissWorld::Update(ECS::Manager& ecs)
{
  // Camera GUI...
  {
    Transform* camTransform = ecs.GetComponent<Transform>(camera);
    FlyCam* flyCam          = ecs.GetComponent<FlyCam>(camera);
    ImGui::Begin("Camera Settings");
    ImGui::InputFloat("Move Speed", &flyCam->moveSpeed);
    ImGui::InputFloat2("Look Speed", glm::value_ptr(flyCam->lookSpeed));
    ImGui::InputFloat3("Position", glm::value_ptr(camTransform->position));
    ImGui::InputFloat3("Rotation", glm::value_ptr(camTransform->rotation));
    ImGui::End();
  }

  // Point light GUI...
  {
    ImGui::Begin("Point Light");
    Transform* transform = ecs.GetComponent<Transform>(pointLight);
    PointLight* light    = ecs.GetComponent<PointLight>(pointLight);
    ImGui::InputFloat3("position", glm::value_ptr(transform->position));
    ImGui::ColorPicker3("colour", glm::value_ptr(light->colour));
    ImGui::InputFloat("range", &light->range);
    ImGui::End();
  }
}
