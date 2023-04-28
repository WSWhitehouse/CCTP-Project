#version 450

#include "../core.glsl"

layout(push_constant) uniform PushConstData
{
  mat4 WVP;
  vec3 colour;
} pushConst;

layout(location = 0) out vec4 outColour;

void main()
{
  outColour = vec4(pushConst.colour.rgb, 1.0);
}
