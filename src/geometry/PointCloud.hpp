#ifndef SNOWFLAKE_POINT_CLOUD_HPP
#define SNOWFLAKE_POINT_CLOUD_HPP

#include "pch.hpp"

// containers
#include "containers/DArray.hpp"

struct Mesh;
struct MeshGeometry;
struct BoundingBox3D;

struct PointCloud
{
  void GenerateFromMesh(const Mesh* mesh);

  [[nodiscard]] BoundingBox3D CalculateBoundingBox(const glm::mat4& transform = glm::identity<glm::mat4>()) const;

  glm::vec3* points = {};
  u32 pointCount    = 0;
};

#endif //SNOWFLAKE_POINT_CLOUD_HPP
