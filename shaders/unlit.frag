#version 450

#include "core.glsl"
#include "modelData.glsl"

layout(set = 2, binding = 0) uniform sampler2DArray texSampler;

layout(push_constant) uniform PushConstData
{
  vec3 colour;
  vec2 texTiling;
} pushConst;

layout(location = 0) out vec4 outColor;
layout(location = 0) in VertOutput vertOut;

void main()
{
  outColor = texture(texSampler, vec3(vertOut.texcoord * pushConst.texTiling, 0.0)) * vec4(vertOut.colour.rgb * pushConst.colour.rgb, 1.0);
}
