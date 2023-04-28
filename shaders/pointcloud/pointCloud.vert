#version 450

#include "pointCloud.glsl"
#include "../core.glsl"
#include "../modelData.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 0) out PcVertOutput vertOut;

void main()
{
  vertOut.position = ModelData.WVP      * vec4(inPosition, 1.0);
  vertOut.worldPos = ModelData.worldMat * vec4(inPosition, 1.0);

  gl_Position = vertOut.position;

  const float dist = length(vertOut.worldPos.xyz - CameraData.position);
  gl_PointSize = 50.0f / dist;
}