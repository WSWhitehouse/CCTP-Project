#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#define MAX_POINT_LIGHTS 1
struct PointLight
{
  vec3 position;
  vec3 colour;
  float range;
};

#endif // LIGHTS_GLSL
