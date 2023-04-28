#version 450

#include "../core.glsl"

layout(push_constant) uniform PushConstData
{
  mat4 WVP;
  vec3 colour;
} pushConst;

layout(location = 0) in vec3 inPosition;

void main()
{
  gl_Position = pushConst.WVP * vec4(inPosition, 1.0);
}
