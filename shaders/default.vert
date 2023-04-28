#version 450

#include "core.glsl"
#include "modelData.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColour;
layout(location = 3) in vec2 inTexcoord;

layout(location = 0) out VertOutput vertOut;

void main()
{
  vertOut.position = ModelData.WVP      * vec4(inPosition, 1.0);
  vertOut.worldPos = ModelData.worldMat * vec4(inPosition, 1.0);
  vertOut.normal   = ModelData.worldMat * vec4(inNormal, 0.0);
  vertOut.colour   = inColour;
  vertOut.texcoord = inTexcoord;

  gl_Position  = vertOut.position;
}