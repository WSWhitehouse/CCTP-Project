#ifndef PBR_GLSL
#define PBR_GLSL

#include "core.glsl"
#include "modelData.glsl"

// NOTE(WSWhitehouse): Most of this code was taken from my implementation of a
// basic PBR material from the Advanced Technologies task 02 (Clay Appearance). 
vec3 PBRCalculateColour(vec3 worldPos, vec3 diffuse, vec3 normal)
{
  // --- Ambient Light --- //
  vec3 ambientLight = FrameData.ambientColour * diffuse;

  // NOTE(WSWhitehouse): If there are no lights in the scene return the ambient light.
  if (FrameData.pointLightCount <= 0) { return ambientLight; }

  // TODO(WSWhitehouse): Support multiple lights, right now its hardcoded to the first one.
  PointLight light = FrameData.pointLights[0];

  // --- Light Values --- //
  const vec3 lightDir      = light.position - worldPos;
  const float lightDist    = length(lightDir);
  const float lightDistSqr = lightDist * lightDist;

  const vec3 N = normalize(normal);
  const vec3 L = normalize(lightDir);
  const vec3 V = normalize(CameraData.position - worldPos);
  const vec3 H = normalize(V + L);

  const float NdotL = max(dot(N, L), 0.0);
  const float NdotH = max(dot(N, H), 0.0);
  const float NdotV = max(dot(N, V), 0.0);
  const float VdotH = max(dot(V, H), 0.0);

  // --- Diffuse Light --- //
  float brightness  = CLAMP01(NdotL); // Blinn
  vec3 diffuseLight = (brightness * light.colour * diffuse) / lightDistSqr;

  //  --- Specular Light --- //
  // Phong
  vec3 R          = reflect(-L, N);
  float specAngle = max(0.0, dot(R, V));
  float specPower = pow(specAngle, 4);

  // NOTE(WSWhitehouse): This ensures that the specular only appears on the correct side of the object...
  // https://forum.unity.com/threads/bug-in-all-specular-shaders-causing-highlights-from-light-sources-behind-objects.73125/#post-470278
  if(NdotL <= 0.001) { specPower = 0.0; }

  vec3 specularLight = (specPower * diffuse) / lightDistSqr;

  // --- Attenuation --- //
  float attenuation = (1.0 / lightDistSqr) * light.range;

  // --- Calculate Final Light --- //
  vec3 finalLight = (diffuseLight + specularLight) * attenuation;
  return ambientLight + finalLight;
}

#endif // PBR_GLSL