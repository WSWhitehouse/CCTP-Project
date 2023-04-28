#ifndef SNOWFLAKE_MESH_HPP
#define SNOWFLAKE_MESH_HPP

#include "pch.hpp"
#include "core/Logging.hpp"

#include "geometry/MeshGeometry.hpp"

// Forward Declarations
struct Vertex;
struct MeshGeometry;
struct MeshNode;
struct BoundingBox3D;
struct Material;

enum class Primitive : u16
{
  QUAD,
  CUBE,
};

struct Mesh
{
  // TODO(WSWhitehouse): Support materials.

  MeshNode* nodeArray         = nullptr;
  MeshGeometry* geometryArray = nullptr;

  u32 nodeCount     = 0;
  u32 geometryCount = 0;

  /**
  * @brief CreateECS a mesh primitive. Do NOT free the returned pointer,
  * primitives are statically allocated to reduce memory usage for
  * simple meshes.
  * @param primitive Primitive type.
  * @return Pointer to primitive mesh, nullptr on failure.
  */
  static const Mesh* CreatePrimitive(Primitive primitive);
};

/** @brief A struct to hold a reference to geometry. */
struct MeshNode
{
  i32 geometryIndex         = -1;
  glm::mat4 transformMatrix = glm::identity<glm::mat4>();
  Material* material        = nullptr;
};


#endif //SNOWFLAKE_MESH_HPP
