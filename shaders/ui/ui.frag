#version 450
#include "ui.glsl"

layout(location = 0) out vec4 outColour;
layout(location = 0) in VertOutput vertOut;

layout(set = 1, binding = 0) uniform sampler2DArray texSampler;

void main()
{
  outColour = texture(texSampler, vec3(vertOut.texcoord.xy, imageData.texIndex)) * vec4(vertOut.colour.xyz, 1.0);
  if (outColour.a <= 0.00001) discard;
}
