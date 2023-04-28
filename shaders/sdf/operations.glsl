#ifndef SDF_OPERATIONS_GLSL
#define SDF_OPERATIONS_GLSL

#include "../core.glsl"

float sdf_Displacement(vec3 pos, float dist, vec3 amount)
{
  float displacement = sin(amount.x * pos.x) *
                       sin(amount.y * pos.y) *
                       sin(amount.z * pos.z);

  return dist + displacement;
}

float sdf_SineWave(float dist, vec3 dir, float freq, float speed, float amplitude)
{
  float x = sin(dir.x * freq + (FrameData.time * speed)) * amplitude;
//  float y = sin(dir.y * freq + (FrameData.time * speed)) * amplitude;
//  float z = sin(dir.z * freq + (FrameData.time * speed)) * amplitude;
//
//  if (dir.x <= 0.01) x = 0.0;
//  if (dir.y <= 0.01) y = 0.0;
//  if (dir.z <= 0.01) z = 0.0;

  return dist - (x);
}

vec3 sdf_TwistX(vec3 pos, float twistAmount)
{
  float c = cos(twistAmount * pos.x);
  float s = sin(twistAmount * pos.x);
  mat2 m  = mat2(c, -s, s, c);

  return vec3(pos.x, (m * pos.yz));
}

vec3 sdf_TwistY(vec3 pos, float twistAmount)
{
  float c = cos(twistAmount * pos.y);
  float s = sin(twistAmount * pos.y);
  mat2 m  = mat2(c, -s, s, c);
  vec2 xz = (m * pos.xz);

  return vec3(xz.x, pos.y, xz.y);
}

vec3 sdf_TwistZ(vec3 pos, float twistAmount)
{
  float c = cos(twistAmount * pos.z);
  float s = sin(twistAmount * pos.z);
  mat2 m  = mat2(c, -s, s, c);

  return vec3((m * pos.xy), pos.z);
}


#endif // SDF_OPERATIONS_GLSL