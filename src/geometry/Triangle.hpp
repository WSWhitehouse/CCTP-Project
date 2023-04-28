#ifndef SNOWFLAKE_TRIANGLE_HPP
#define SNOWFLAKE_TRIANGLE_HPP

#include "pch.hpp"
#include "containers/FArray.hpp"

struct GPUTriangle
{
  alignas(16) glm::vec3 a;
  alignas(16) glm::vec3 b;
  alignas(16) glm::vec3 c;

  alignas(16) glm::vec3 normal;
};

struct Triangle
{
  FArray<glm::vec3, 3> vertices = {};

  [[nodiscard]] INLINE f32 SignedDistanceFromPoint(glm::vec3 point) const
  {
    // https://iquilezles.org/articles/distfunctions/

    const glm::vec3& a = vertices[0];
    const glm::vec3& b = vertices[1];
    const glm::vec3& c = vertices[2];

    glm::vec3 ba = b     - a;
    glm::vec3 pa = point - a;
    glm::vec3 cb = c     - b;
    glm::vec3 pb = point - b;
    glm::vec3 ac = a     - c;
    glm::vec3 pc = point - c;

    const glm::vec3 normal = CalculateNormal();

    if (glm::sign(glm::dot(glm::cross(ba, normal), pa)) +
        glm::sign(glm::dot(glm::cross(cb, normal), pb)) +
        glm::sign(glm::dot(glm::cross(ac, normal), pc)) < 2.0)
    {
      glm::vec3 min1 = ba * glm::clamp(glm::dot(ba,pa) / glm::dot(ba, ba), 0.0f ,1.0f) - pa;
      glm::vec3 min2 = cb * glm::clamp(glm::dot(cb,pb) / glm::dot(cb, cb), 0.0f, 1.0f) - pb;
      glm::vec3 min3 = ac * glm::clamp(glm::dot(ac,pc) / glm::dot(ac, ac), 0.0f, 1.0f) - pc;

      return glm::sqrt(MIN(MIN(dot(min1, min1), dot(min2, min2)), dot(min3, min3)));
    }

    return glm::sqrt(dot(normal, pa) * dot(normal, pa) / dot(normal, normal));
  }

  [[nodiscard]] INLINE glm::vec3 CalculateNormal() const
  {
    return CalculateNormal(vertices[0], vertices[1], vertices[2]);
  }

  [[nodiscard]] static INLINE glm::vec3 CalculateNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
  {
    // https://stackoverflow.com/a/23709352/13195883
    // https://www.khronos.org/opengl/wiki/Calculating_a_Surface_Normal
    const glm::vec3 u = b - a;
    const glm::vec3 v = a - c;
    return glm::normalize(glm::cross(u, v));
  }

  [[nodiscard]] INLINE glm::vec3 CalculateCentroid() const
  {
    return (vertices[0] + vertices[1] + vertices[2]) / 3.0f;
  }

  [[nodiscard]] INLINE f32 CalculateArea() const
  {
    const glm::vec3& a = vertices[0];
    const glm::vec3& b = vertices[1];
    const glm::vec3& c = vertices[2];

    f32 abx = b.x - a.x; f32 acx = c.x - a.x;
    f32 aby = b.y - a.y; f32 acy = c.y - a.y;
    f32 abz = b.z - a.z; f32 acz = c.z - a.z;

    f32 nx = aby * acz - abz * acy;
    f32 ny = abz * acx - abx * acz;
    f32 nz = abx * acy - aby * acx;

    return glm::sqrt(nx * nx + ny * ny + nz * nz) / 2.0f;
  }
};

#endif //SNOWFLAKE_TRIANGLE_HPP
