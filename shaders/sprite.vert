#version 450

#include "core.glsl"
#include "sprite.glsl"
#include "primitives/quad.glsl"

layout(set = 1, binding = 0) uniform UBOSpriteData
{
  mat4 WVP;
  mat4 worldMat;
  vec2 textureSize;
  vec2 size;
} SpriteData;

layout(location = 0) out SpriteVertOutput vertOut;

void main()
{
  vec2 size = SpriteData.size;
  const float aspect = SpriteData.textureSize.y / SpriteData.textureSize.x;

  if (aspect > 1.0) size.y *= aspect;
  else              size.x /= aspect;

  const vec4 inPosition = vec4(quadVertices[gl_VertexIndex] + vec3(0.0, 0.5, 0.0), 1.0) * vec4(size.xy, 1.0, 1.0);
  const vec4 inNormal   = vec4(quadNormals[gl_VertexIndex],  0.0);
  const vec2 inTexcoord = quadTexcoords[gl_VertexIndex];

  vertOut.position = SpriteData.WVP      * inPosition;
  vertOut.worldPos = SpriteData.worldMat * inPosition;
  vertOut.normal   = SpriteData.worldMat * inNormal;
  vertOut.texcoord = inTexcoord;

  gl_Position  = vertOut.position;
}
