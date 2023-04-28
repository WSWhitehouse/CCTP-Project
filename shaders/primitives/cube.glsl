#ifndef CUBE_GLSL
#define CUBE_GLSL

#define CUBE_INDEX_COUNT 36
#define CUBE_VERT_COUNT 24

const int cubeIndices[CUBE_INDEX_COUNT] = int[]
(
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
);

const vec3 cubeVertices[CUBE_VERT_COUNT] = vec3[]
(
  // Front Face
  vec3(-0.5f, -0.5f, -0.5f),
  vec3(-0.5f,  0.5f, -0.5f),
  vec3( 0.5f,  0.5f, -0.5f),
  vec3( 0.5f, -0.5f, -0.5f),

  // Back Face
  vec3(-0.5f, -0.5f,  0.5f),
  vec3( 0.5f, -0.5f,  0.5f),
  vec3( 0.5f,  0.5f,  0.5f),
  vec3(-0.5f,  0.5f,  0.5f),

  // Bottom Face
  vec3(-0.5f,  0.5f, -0.5f),
  vec3(-0.5f,  0.5f,  0.5f),
  vec3( 0.5f,  0.5f,  0.5f),
  vec3( 0.5f,  0.5f, -0.5f),

  // Top Face
  vec3(-0.5f, -0.5f, -0.5f),
  vec3( 0.5f, -0.5f, -0.5f),
  vec3( 0.5f, -0.5f,  0.5f),
  vec3(-0.5f, -0.5f,  0.5f),

  // Left Face
  vec3(-0.5f, -0.5f,  0.5f),
  vec3(-0.5f,  0.5f,  0.5f),
  vec3(-0.5f,  0.5f, -0.5f),
  vec3(-0.5f, -0.5f, -0.5f),

  // Right Face
  vec3( 0.5f, -0.5f, -0.5f),
  vec3( 0.5f,  0.5f, -0.5f),
  vec3( 0.5f,  0.5f,  0.5f),
  vec3( 0.5f, -0.5f,  0.5f)
);

const vec3 cubeNormals[CUBE_VERT_COUNT] = vec3[]
(
  // Front Face
  vec3(-1.0f, -1.0f, -1.0f),
  vec3(-1.0f,  1.0f, -1.0f),
  vec3( 1.0f,  1.0f, -1.0f),
  vec3( 1.0f, -1.0f, -1.0f),

  // Back Face
  vec3(-1.0f, -1.0f, 1.0f),
  vec3( 1.0f, -1.0f, 1.0f),
  vec3( 1.0f,  1.0f, 1.0f),
  vec3(-1.0f,  1.0f, 1.0f),

  // Bottom Face
  vec3(-1.0f, 1.0f, -1.0f),
  vec3(-1.0f, 1.0f,  1.0f),
  vec3( 1.0f, 1.0f,  1.0f),
  vec3( 1.0f, 1.0f, -1.0f),

  // Top Face
  vec3(-1.0f, -1.0f, -1.0f),
  vec3( 1.0f, -1.0f, -1.0f),
  vec3( 1.0f, -1.0f,  1.0f),
  vec3(-1.0f, -1.0f,  1.0f),

  // Left Face
  vec3(-1.0f, -1.0f,  1.0f),
  vec3(-1.0f,  1.0f,  1.0f),
  vec3(-1.0f,  1.0f, -1.0f),
  vec3(-1.0f, -1.0f, -1.0f),

  // Right Face
  vec3(1.0f, -1.0f, -1.0f),
  vec3(1.0f,  1.0f, -1.0f),
  vec3(1.0f,  1.0f,  1.0f),
  vec3(1.0f, -1.0f,  1.0f)
);

const vec2 cubeTexcoords[CUBE_VERT_COUNT] = vec2[]
(
  // Front Face
  vec2(0.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),
  vec2(1.0f, 0.0f),

  // Back Face
  vec2(1.0f, 0.0f),
  vec2(0.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),

  // Bottom Face
  vec2(0.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),
  vec2(1.0f, 0.0f),

  // Top Face
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),
  vec2(1.0f, 0.0f),
  vec2(0.0f, 0.0f),

  // Left Face
  vec2(0.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),
  vec2(1.0f, 0.0f),

  // Right Face
  vec2(0.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 1.0f),
  vec2(1.0f, 0.0f)
);

#endif // CUBE_GLSL