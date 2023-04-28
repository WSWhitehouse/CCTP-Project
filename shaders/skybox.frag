#version 450

#include "core.glsl"

layout(set = 1, binding = 1) uniform samplerCube skyboxSampler;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragTexcoord;

void main()
{
  outColor = texture(skyboxSampler, fragTexcoord) * vec4(fragColor, 1.0);
}