#version 450

#include "pointCloud.glsl"
#include "../core.glsl"
#include "../modelData.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in PcVertOutput vertOut;

void main()
{
  outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
