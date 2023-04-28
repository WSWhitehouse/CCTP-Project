#include "geometry/Mesh.hpp"

// core
#include "core/Logging.hpp"

// geometry
#include "geometry/BoundingBox3D.hpp"
#include "geometry/PrimitiveData.hpp"

/**
* @brief Initialises the Mesh structs statically with the appropriate vertex and index
* arrays. This should be placed inside a function; see `Mesh::CreatePrimitive()`.
* @param meshName Name of the Mesh struct that's created. This is also used to name the
* appropriate node and geometry structs ("meshName_node" and "meshName_geometry").
* @param vertices The vertex array for this primitive - *must* be statically allocated.
* @param indices The index array for this primitive - *must* be statically allocated.
*/
#define INIT_STATIC_PRIMITIVE(meshName, vertices, indices) \
  static MeshGeometry meshName##_geometry                  \
    {                                                      \
      .vertexArray = &vertices[0],                         \
      .indexArray  = &indices[0],                          \
      .vertexCount = ARRAY_SIZE(vertices),                 \
      .indexCount  = ARRAY_SIZE(indices)                   \
    };                                                     \
                                                           \
  static MeshNode meshName##_node                          \
    {                                                      \
      .geometryIndex   = 0,                                \
      .transformMatrix = glm::mat4(1.0f)                   \
    };                                                     \
                                                           \
  static Mesh meshName                                     \
    {                                                      \
      .nodeArray     = &meshName##_node,                   \
      .geometryArray = &meshName##_geometry,               \
      .nodeCount     = 1,                                  \
      .geometryCount = 1,                                  \
    };

const Mesh* Mesh::CreatePrimitive(Primitive primitive)
{
  INIT_STATIC_PRIMITIVE(quadMesh, QuadVertices, QuadIndices);
  INIT_STATIC_PRIMITIVE(cubeMesh, CubeVertices, CubeIndices);

  switch (primitive)
  {
    case Primitive::QUAD: return &quadMesh;
    case Primitive::CUBE: return &cubeMesh;

    default:
    {
      LOG_ERROR("Unhandled primitive case!");
      break;
    }
  }

  return nullptr;
}
