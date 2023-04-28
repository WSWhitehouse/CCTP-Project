#version 450

#include "core.glsl"
#include "primitives/cube.glsl"

layout(set = 1, binding = 0) uniform UBOSkyboxData
{
  mat4 WVP;
  vec3 colour;
} SkyboxData;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragTexcoord;

void main()
{
  const int vertIndex   = cubeIndices[gl_VertexIndex];
  const vec4 inPosition = vec4(cubeVertices[vertIndex], 0.0);

  gl_Position  = (SkyboxData.WVP * inPosition).xyzz;
  fragColor    = SkyboxData.colour;
  fragTexcoord = inPosition.xyz;
}