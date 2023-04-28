#ifndef SNOWFLAKE_TRIANGULATION_HPP
#define SNOWFLAKE_TRIANGULATION_HPP

#include "pch.hpp"
#include "containers/FArray.hpp"

typedef FArray<glm::vec3, 3> Triangle;

// Forward Declarations
struct Vertex;

namespace Triangulation
{

  std::vector<Triangle> EarClipping3D(const glm::vec3* points, u32 pointCount);

  std::vector<Vertex> EarClipping3D(const Vertex* vertices, u32 vertexCount);

} // namespace Triangulation

#endif //SNOWFLAKE_TRIANGULATION_HPP
