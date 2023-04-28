#ifndef SNOWFLAKE_PRIMITIVE_DATA_HPP
#define SNOWFLAKE_PRIMITIVE_DATA_HPP

#include "pch.hpp"
#include "geometry/Vertex.hpp"

// -- PRIMITIVE::QUAD ------------------------------------------------------------------------------------------- //

static inline Vertex QuadVertices[] =
{
  //          Position                        Texcoord               Normals
  { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, -1.0f, -1.0f) },
  { glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f,  1.0f, -1.0f) },
  { glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3( 1.0f,  1.0f, -1.0f) },
  { glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3( 1.0f, -1.0f, -1.0f) }
};

static inline u16 QuadIndices[] =
{
  0, 1, 2,
  0, 2, 3,
};

// -- PRIMITIVE::CUBE ------------------------------------------------------------------------------------------- //

static inline Vertex CubeVertices[] =
{
  //          Position                        Texcoord               Normals
  // Front Face
  { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, -1.0f, -1.0f) },
  { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f,  1.0f, -1.0f) },
  { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 0.0f), glm::vec3( 1.0f,  1.0f, -1.0f) },
  { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(1.0f, 1.0f), glm::vec3( 1.0f, -1.0f, -1.0f) },

  // Back Face
  { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, 1.0f) },
  { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 1.0f), glm::vec3( 1.0f, -1.0f, 1.0f) },
  { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(0.0f, 0.0f), glm::vec3( 1.0f,  1.0f, 1.0f) },
  { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f), glm::vec3(-1.0f,  1.0f, 1.0f) },

  // Bottom Face
  { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, 1.0f, -1.0f) },
  { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f, 1.0f,  1.0f) },
  { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f), glm::vec3( 1.0f, 1.0f,  1.0f) },
  { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 1.0f), glm::vec3( 1.0f, 1.0f, -1.0f) },

  // Top Face
  { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f, -1.0f, -1.0f) },
  { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(1.0f, 0.0f), glm::vec3( 1.0f, -1.0f, -1.0f) },
  { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 1.0f), glm::vec3( 1.0f, -1.0f,  1.0f) },
  { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, -1.0f,  1.0f) },

  // Left Face
  { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec2(0.0f, 1.0f), glm::vec3(-1.0f, -1.0f,  1.0f) },
  { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec2(0.0f, 0.0f), glm::vec3(-1.0f,  1.0f,  1.0f) },
  { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec2(1.0f, 0.0f), glm::vec3(-1.0f,  1.0f, -1.0f) },
  { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec2(1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, -1.0f) },

  // Right Face
  { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec2(0.0f, 1.0f), glm::vec3(1.0f, -1.0f, -1.0f) },
  { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec2(0.0f, 0.0f), glm::vec3(1.0f,  1.0f, -1.0f) },
  { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec2(1.0f, 0.0f), glm::vec3(1.0f,  1.0f,  1.0f) },
  { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec2(1.0f, 1.0f), glm::vec3(1.0f, -1.0f,  1.0f) }
};

static inline u16 CubeIndices[] =
{
  // Front Face
  0,  1,  2,
  0,  2,  3,

  // Back Face
  4,  5,  6,
  4,  6,  7,

  // Bottom Face
  8,  9, 10,
  8, 10, 11,

  // Top Face
  12, 13, 14,
  12, 14, 15,

  // Left Face
  16, 17, 18,
  16, 18, 19,

  // Right Face
  20, 21, 22,
  20, 22, 23
};


#endif //SNOWFLAKE_PRIMITIVE_DATA_HPP
