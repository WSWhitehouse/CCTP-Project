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
static DArray<TreeNode> meshEntities = {};
static Entity sdfVoxelGrid;
//static Entity sdfRenderer;
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

//    SkyboxCreateInfo createInfo =
//      {
//        .texturePathPosX = "data/yokohama/posx.jpg",
//        .texturePathNegX = "data/yokohama/negx.jpg",
//        .texturePathPosY = "data/yokohama/posy.jpg",
//        .texturePathNegY = "data/yokohama/negy.jpg",
//        .texturePathPosZ = "data/yokohama/posz.jpg",
//        .texturePathNegZ = "data/yokohama/negz.jpg"
//      };

//    Skybox* skybox = ecs.AddComponent<Skybox>(camera);
//    ComponentFactory::SkyboxCreate(skybox, createInfo);
  }

  // TODO(WSWhitehouse): Clean up mesh... its currently a mem leak.
//  Mesh* testMesh = AssetDatabase::LoadMesh("data/bunny.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/test-obj.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/low-poly-sphere.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/sphere.glb");
  const Mesh* testMesh = AssetDatabase::LoadMesh("data/stanford-bunny-high-res.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/sponza/sponza.glb");
//  const Mesh* testMesh = AssetDatabase::LoadMesh("data/monkey.glb");
  if (testMesh == nullptr) ABORT(ABORT_CODE_ASSET_FAILURE);

  LOG_DEBUG("Vertex Count: %u, Index Count: %u, Triangle Count: %u",
            testMesh->geometryArray[0].vertexCount, testMesh->geometryArray[0].indexCount, testMesh->geometryArray[0].indexCount / 3);

//  sdfRenderer = ecs.CreateEntity();
//  {
//    Transform* transform = ecs.AddComponent<Transform>(sdfRenderer);
//    transform->position  = {0.0f, 0.0f, 0.0f};
//    transform->scale     = {1.0f, 1.0f, 1.0f};
//
//    SdfRenderer* renderer = ecs.AddComponent<SdfRenderer>(sdfRenderer);
//
//    {
//      PROFILE_FUNC
//      ComponentFactory::SdfRendererCreate(ecs, renderer, testMesh);
//    }
//  }

//  PointCloud cloud = {};
//  cloud.GenerateFromMesh(testMesh);

//  ECS::Entity pointCloud = ecs.CreateEntity();
//  {
//    Transform* transform = ecs.AddComponent<Transform>(pointCloud);
//    transform->position  = {0.0f, 0.0f, 4.5f};
//    transform->scale     = {1.0f, 1.0f, 1.0f};
//
//    PointCloudRenderer* renderer = ecs.AddComponent<PointCloudRenderer>(pointCloud);
//    ComponentFactory::PointCloudRendererCreate(renderer, &cloud);
//  }

//  sdfRenderer = ecs.CreateEntity();
//  {
//    Transform* transform = ecs.AddComponent<Transform>(sdfRenderer);
//    transform->position  = {0.0f, 0.0f, 0.0f};
//    transform->scale     = {1.0f, 1.0f, 1.0f};
//
//    SdfPointCloudRenderer* renderer = ecs.AddComponent<SdfPointCloudRenderer>(sdfRenderer);
//    ComponentFactory::SdfPointCloudRendererCreate(renderer, &cloud);
//  }

  pointLight = ecs.CreateEntity();
  {
    Transform* transform = ecs.AddComponent<Transform>(pointLight);
    transform->position = { 2.0f, 0.0f, 3.0f };

    PointLight* light = ecs.AddComponent<PointLight>(pointLight);
    light->range  = 5.0f;
    light->colour = {1.0f, 0.0f, 0.8f};
  }

  SdfVoxelGrid::CreateComputePipeline();
  sdfVoxelGrid = ecs.CreateEntity();
  {
    Transform* transform = ecs.AddComponent<Transform>(sdfVoxelGrid);
    transform->position = { 0.0f, 0.0f, 5.0f };
    transform->rotation = { 0.0f, 180.0f, 0.0f};

    SdfVoxelGrid* voxelGrid = ecs.AddComponent<SdfVoxelGrid>(sdfVoxelGrid);

//    glm::vec3 cellCount = glm::vec3{1024, 1024, 1024};
    glm::vec3 cellCount = glm::vec3{512, 512, 512};
//    glm::vec3 cellCount = glm::vec3{256, 256, 256};
//    glm::vec3 cellCount = glm::vec3{128, 128, 128};
//    glm::vec3 cellCount = glm::vec3{64, 64, 64};
//    glm::vec3 cellCount = glm::vec3{32, 32, 32};
    SdfVoxelGrid::Create(voxelGrid, true, testMesh, cellCount);
  }
  SdfVoxelGrid::CleanUpComputePipeline();
}

void DissWorld::Shutdown(ECS::Manager& ecs)
{
  SdfVoxelGrid* voxelGrid = ecs.GetComponent<SdfVoxelGrid>(sdfVoxelGrid);
  SdfVoxelGrid::Release(voxelGrid);

//  SdfRenderer* renderer = ecs.GetComponent<SdfRenderer>(sdfRenderer);
//  ComponentFactory::SdfRendererDestroy(renderer);

//  for (u32 i = 0; i < meshEntities.Size(); ++i)
//  {
//    MeshRenderer* meshRenderer = ecs.GetComponent<MeshRenderer>(meshEntities[i].entity);
//    ComponentFactory::MeshRendererDestroy(meshRenderer);
//  }

//  meshEntities.Destroy();

  Skybox* skybox = ecs.GetComponent<Skybox>(camera);
  ComponentFactory::SkyboxDestroy(skybox);
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

//  {
//    Transform* transform = ecs.GetComponent<Transform>(sdfRenderer);
//
//    ImGui::Begin("SDF Renderer");
//    ImGui::InputFloat3("Position", glm::value_ptr(transform->position));
//    ImGui::SliderFloat3("Rotation", glm::value_ptr(transform->rotation), 0.0f, 360.0f);
//    ImGui::InputFloat3("Scale", glm::value_ptr(transform->scale));
//    ImGui::End();
//  }

  // Voxel Grid Gizmos
  {
//    const Transform* transform    = ecs.GetComponent<Transform>(sdfVoxelGrid);
//    SdfVoxelGrid* voxelGrid = ecs.GetComponent<SdfVoxelGrid>(sdfVoxelGrid);
//    Gizmos::DrawWireCube(transform->position, voxelGrid->gridExtent * 0.5f, glm::identity<glm::quat>());
  }

  for (u32 i = 0; i < meshEntities.Size() && i < 10; ++i)
  {
    Transform* transform       = ecs.GetComponent<Transform>(meshEntities[i].entity);
    MeshRenderer* meshRenderer = ecs.GetComponent<MeshRenderer>(meshEntities[i].entity);

    char name[40] = "Mesh";
    sprintf(name, "Mesh %u %s", i, meshEntities[i].isLeaf ? "(Geometry)" : "(Plane)");

    ImGui::Begin(name);
    ImGui::Checkbox("Render", &meshRenderer->renderMesh);
    ImGui::InputFloat3("Position", glm::value_ptr(transform->position));
    ImGui::SliderFloat3("Rotation", glm::value_ptr(transform->rotation), 0.0f, 360.0f);
    ImGui::InputFloat3("Scale", glm::value_ptr(transform->scale));
    ImGui::End();
  }
}
