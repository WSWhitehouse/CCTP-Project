#version 450
#include "ui.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColour;
layout(location = 3) in vec2 inTexcoord;

layout(location = 0) out VertOutput vertOut;

void main()
{
  const vec2 zeroToOne = (vec2(1.0, -1.0) * inPosition.xy) + 0.5;
  const vec2 pixelPos  = (imageData.pos + (zeroToOne * imageData.size * imageData.scale));
  const vec2 clipSpace = (pixelPos / TARGET_SCREEN_SIZE * 2.0 - 1.0);

  vertOut.position = clipSpace;
  vertOut.colour   = imageData.colour;
  vertOut.texcoord = inTexcoord;

  gl_Position = vec4(vertOut.position.xy, imageData.zOrder, 1.0);
}
