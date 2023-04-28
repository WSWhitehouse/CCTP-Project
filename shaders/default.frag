#version 450

#include "core.glsl"
#include "modelData.glsl"
#include "pbr.glsl"

layout(set = 3, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstData
{
    vec3 colour;
} pushConst;

layout(location = 0) out vec4 outColour;
layout(location = 0) in VertOutput vertOut;

void main()
{
    vec3 diffuse = texture(texSampler, vertOut.texcoord).rgb * vertOut.colour * pushConst.colour;
    outColour = vec4(PBRCalculateColour(vertOut.worldPos.xyz, diffuse, vertOut.normal.xyz), 1.0);
}