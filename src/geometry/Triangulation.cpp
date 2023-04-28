#include "geometry/Triangulation.hpp"

// geometry
#include "geometry/Vertex.hpp"

static INLINE glm::vec3 ComputeTriangleNormal(const Triangle& triangle)
{
  return glm::normalize(glm::cross(triangle[1] - triangle[0], triangle[2] - triangle[0]));
}

static INLINE f32 ComputeTriangleArea(const Triangle& triangle, const glm::vec3& normal)
{
  // Compute the length of the two sides of the triangle
  glm::vec3 side1 = triangle[1] - triangle[0];
  glm::vec3 side2 = triangle[2] - triangle[0];
  float length1 = glm::length(side1);
  float length2 = glm::length(side2);

  // Compute the angle between the two sides of the triangle
  float cosTheta = glm::dot(side1, side2) / (length1 * length2);
  float sinTheta = glm::sqrt(1.0f - cosTheta * cosTheta);

  // Compute the area of the triangle using the lengths of the sides and the sine of the angle between them
  float area = length1 * length2 * sinTheta / 2.0f;

  // If the normal of the triangle points in the opposite direction to the side vector, the area should be negative
  if (glm::dot(normal, glm::cross(side1, side2)) < 0.0f)
  {
    area = -area;
  }

  return area;
}

static INLINE b8 IsEar3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                         const glm::vec3* points, u32 pointCount, const std::vector<Triangle>& foundTriangles)
{
  const glm::vec3 normal = ComputeTriangleNormal(Triangle{a, b, c});

  for (u32 i = 0; i < foundTriangles.size(); ++i)
  {
    const Triangle& tri = foundTriangles[i];

    if (tri[0] == a || tri[1] == a || tri[2] == a)
    {
      const glm::vec3 adjacentNormal = ComputeTriangleNormal(tri);
      if (glm::dot(normal, adjacentNormal) <= 0.0f) return false;
    }
  }

  for (u32 i = 0; i < pointCount; ++i)
  {
    const glm::vec3& vertex = points[i];

    // Don't compare the vertex against itself...
    if (vertex == a || vertex == b || vertex == c) continue;

    // Compute the area of the three triangles formed by the vertex and the edges of the current triangle
    f32 area1 = ComputeTriangleArea(Triangle{a, b, vertex}, normal);
    f32 area2 = ComputeTriangleArea(Triangle{b, c, vertex}, normal);
    f32 area3 = ComputeTriangleArea(Triangle{c, a, vertex}, normal);

    // If the sum of the areas is less than or equal to 0, the vertex is not an ear
    if (area1 + area2 + area3 <= 0.0f) return false;
  }

  return true;
}

static INLINE b8 IsEar3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                         const Vertex* vertices, u32 vertexCount, const std::vector<Vertex>& foundTriangles)
{
  const glm::vec3 normal = ComputeTriangleNormal(Triangle{a, b, c});

  for (u32 i = 0; i < foundTriangles.size(); i+=3)
  {
    // Get triangle verts indices...
    const u32 index0 = i + 0;
    const u32 index1 = i + 1;
    const u32 index2 = i + 2;

    // Get triangle verts...
    const glm::vec3& vert0 = foundTriangles[index0].position;
    const glm::vec3& vert1 = foundTriangles[index1].position;
    const glm::vec3& vert2 = foundTriangles[index2].position;

    if (vert0 == a || vert1 == a || vert2 == a)
    {
      const glm::vec3 adjacentNormal = ComputeTriangleNormal(Triangle{vert0, vert1, vert2});
      if (glm::dot(normal, adjacentNormal) <= 0.0f) return false;
    }
  }

  for (u32 i = 0; i < vertexCount; ++i)
  {
    const glm::vec3& vertex = vertices[i].position;

    // Don't compare the vertex against itself...
    if (vertex == a || vertex == b || vertex == c) continue;

    // Compute the area of the three triangles formed by the vertex and the edges of the current triangle
    f32 area1 = ComputeTriangleArea(Triangle{a, b, vertex}, normal);
    f32 area2 = ComputeTriangleArea(Triangle{b, c, vertex}, normal);
    f32 area3 = ComputeTriangleArea(Triangle{c, a, vertex}, normal);

    // If the sum of the areas is less than or equal to 0, the vertex is not an ear
    if (area1 + area2 + area3 <= 0.0f) return false;
  }

  return true;
}

std::vector<Triangle> Triangulation::EarClipping3D(const glm::vec3* points, u32 pointCount)
{
  std::vector<Triangle> result;
  result.reserve(pointCount - 2);

  std::vector<u32> indices(pointCount);
  for (u32 i = 0; i < pointCount; ++i)
  {
    indices[i] = i;
  }

  while (indices.size() > 3)
  {
    b8 foundEar = false;

    for (u32 i = 0; i < indices.size(); ++i)
    {
      u32 prev = (i == 0) ? indices.size() - 1 : i - 1;
      u32 next = (i == indices.size() - 1) ? 0 : i + 1;

      const glm::vec3& a = points[indices[prev]];
      const glm::vec3& b = points[indices[i]];
      const glm::vec3& c = points[indices[next]];

      if (IsEar3D(a, b, c, points, pointCount, result))
      {
        Triangle triangle = { a, b, c };
        result.push_back(triangle);

        indices.erase(indices.begin() + i);

        foundEar = true;
        break;
      }
    }

    if (!foundEar)
    {
      break;
    }
  }

  if (indices.size() == 3)
  {
    Triangle triangle =
      {
        points[indices[0]],
        points[indices[1]],
        points[indices[2]]
      };
    result.push_back(triangle);
  }

  return result;
}

std::vector<Vertex> Triangulation::EarClipping3D(const Vertex* vertices, u32 vertexCount)
{
  std::vector<Vertex> result;
  result.reserve(vertexCount - 2);

  std::vector<u32> indices(vertexCount);
  for (u32 i = 0; i < vertexCount; ++i)
  {
    indices[i] = i;
  }

  while (indices.size() > 3)
  {
    b8 foundEar = false;

    for (u32 i = 0; i < indices.size(); ++i)
    {
      const u32 prev = (i == 0) ? indices.size() - 1 : i - 1;
      const u32 next = (i == indices.size() - 1) ? 0 : i + 1;

      const u32 prevIndex = indices[prev];
      const u32 currIndex = indices[i];
      const u32 nextIndex = indices[next];

      const Vertex& vertA = vertices[prevIndex];
      const Vertex& vertB = vertices[currIndex];
      const Vertex& vertC = vertices[nextIndex];

      const glm::vec3& a = vertA.position;
      const glm::vec3& b = vertB.position;
      const glm::vec3& c = vertC.position;

      if (IsEar3D(a, b, c, vertices, vertexCount, result))
      {
        result.push_back(vertA);
        result.push_back(vertB);
        result.push_back(vertC);

        indices.erase(indices.begin() + i);

        foundEar = true;
        break;
      }
    }

    if (!foundEar) break;
  }

  if (indices.size() == 3)
  {
    result.push_back(vertices[indices[0]]);
    result.push_back(vertices[indices[1]]);
    result.push_back(vertices[indices[2]]);
  }

  return result;
}