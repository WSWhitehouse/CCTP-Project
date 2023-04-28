#ifndef SNOWFLAKE_COMPONENT_FACTORY_HPP
#define SNOWFLAKE_COMPONENT_FACTORY_HPP

#include "pch.hpp"
#include "ecs/ECS.hpp"
#include "ecs/ComponentCreateInfo.hpp"

// Forward Declarations
struct Mesh;
struct MeshGeometry;
struct MeshRenderer;
struct PointCloudRenderer;
struct PointCloud;
struct SdfRenderer;
struct SdfPointCloudRenderer;
struct Sprite;
struct UIImage;
struct Skybox;

/**
* @brief A set of utility functions to create/destroy commonly used components
* that require some set up and initialisation. For example the MeshRenderer.
* Implicit valid usage: All component pointers passed as parameters must not be
* nullptr, and must point to a valid component of that type.
*/
namespace ComponentFactory
{

  // --- MESH RENDERER --- //
  void MeshRendererCreate(MeshRenderer* meshRenderer, const Mesh* mesh);
  void MeshRendererCreate(MeshRenderer* meshRenderer, const MeshGeometry& mesh);
  void MeshRendererDestroy(MeshRenderer* meshRenderer, b8 destroyMaterials = false);

  // --- POINT CLOUD RENDERER --- //
  void PointCloudRendererCreate(PointCloudRenderer* pointCloudRenderer, const PointCloud* pointCloud);
  void PointCloudRendererDestroy(PointCloudRenderer* pointCloudRenderer);

  // --- SPRITE --- //
  void SpriteCreate(Sprite* sprite, const SpriteCreateInfo& createInfo);
  void SpriteDestroy(Sprite* sprite, b8 immediate = true);

  // --- UI IMAGE --- //
  void UIImageCreate(UIImage* image, const UIImageCreateInfo& createInfo);
  void UIImageDestroy(UIImage* image);

  // --- SKYBOX --- //
  void SkyboxCreate(Skybox* skybox, const SkyboxCreateInfo& createInfo);
  void SkyboxDestroy(Skybox* skybox);

  // --- SDF RENDERER --- //
  void SdfRendererCreate(ECS::Manager& ecs, SdfRenderer* sdfRenderer, const Mesh* mesh);
  void SdfRendererDestroy(SdfRenderer* sdfRenderer);

  // --- SDF POINT CLOUD RENDERER --- //
  void SdfPointCloudRendererCreate(SdfPointCloudRenderer* sdfPointCloudRenderer, const PointCloud* pointCloud);
  void SdfPointCloudRendererDestroy(SdfPointCloudRenderer* sdfPointCloudRenderer);

} // namespace ComponentFactory


#endif //SNOWFLAKE_COMPONENT_FACTORY_HPP
