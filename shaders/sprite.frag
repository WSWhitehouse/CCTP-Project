#version 450

#include "core.glsl"
#include "sprite.glsl"

layout(set = 1, binding = 1) uniform sampler2DArray texSampler;

layout(push_constant) uniform PushConstData
{
  vec3 colour;
  vec2 uvMultiplier;
  float textureIndex;
} pushConst;

layout(location = 0) out vec4 outColor;
layout(location = 0) in SpriteVertOutput vertOut;

void main()
{
  const vec3 uv = vec3(vertOut.texcoord.xy * pushConst.uvMultiplier, pushConst.textureIndex);
  outColor = texture(texSampler, uv) * vec4(pushConst.colour.rgb, 1.0);
}
