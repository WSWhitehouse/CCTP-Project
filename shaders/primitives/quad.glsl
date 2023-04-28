#ifndef QUAD_GLSL
#define QUAD_GLSL

#define QUAD_VERT_COUNT 6

const vec3 quadVertices[QUAD_VERT_COUNT] = vec3[]
(
  vec3(-0.5f, -0.5f, 0.0f),
  vec3(-0.5f,  0.5f, 0.0f),
  vec3( 0.5f,  0.5f, 0.0f),
  vec3(-0.5f, -0.5f, 0.0f),
  vec3( 0.5f,  0.5f, 0.0f),
  vec3( 0.5f, -0.5f, 0.0f)
);

const vec3 quadNormals[QUAD_VERT_COUNT] = vec3[]
(
  vec3(-1.0f, -1.0f, -1.0f),
  vec3(-1.0f,  1.0f, -1.0f),
  vec3( 1.0f,  1.0f, -1.0f),
  vec3(-1.0f, -1.0f, -1.0f),
  vec3( 1.0f,  1.0f, -1.0f),
  vec3( 1.0f, -1.0f, -1.0f)
);

const vec2 quadTexcoords[QUAD_VERT_COUNT] = vec2[]
(
  vec2(0.0f, 1.0f),
  vec2(0.0f, 0.0f),
  vec2(1.0f, 0.0f),
  vec2(0.0f, 1.0f),
  vec2(1.0f, 0.0f),
  vec2(1.0f, 1.0f)
);

#endif // QUAD_GLSL