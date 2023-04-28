/*
 * @brief This is the core glsl file that should be included in all shader
 * files, it includes the main uniform buffers and utility functions.
 *
 * Layout Binding Overview:
 *  - Binding 0 = FrameData
 *  - Binding 1 = CameraData
 *  - Binding 2 = ModelData
 */

#ifndef SHADER_CORE_GLSL
#define SHADER_CORE_GLSL

#include "lights.glsl"

smooth struct VertOutput
{
  vec4 position;
  vec4 worldPos;
  vec4 normal;
  vec3 colour;
  vec2 texcoord;
};

/**
 * @brief Application and frame related data. Updated once per-frame.
 */
layout(set = 0, binding = 0) uniform UBOFrameData
{
  PointLight pointLights[MAX_POINT_LIGHTS];
  int pointLightCount;
  vec3 ambientColour;
  float time;
  float sinTime;

} FrameData;

/**
 * @brief Camera related data. Updated per-camera.
 */
layout(set = 0, binding = 1) uniform UBOCameraData
{
  vec3 position;
  mat4 viewMat;
  mat4 projMat;
  mat4 invViewMat;
  mat4 invProjMat;
} CameraData;

/**
 * @brief Pi Constant (3.1415...)
 */
#define PI 3.1415926535897932384626433832795

/**
* @brief Clamp value between a low and high value.
* @param val Value to clamp.
* @param low Minimum value.
* @param high Maximum value.
*/
#define CLAMP(val, low, high) (min((high), max((low), (val))))

/**
* @brief Clamp value between 0.0 and 1.0
* @param val Value to clamp.
*/
#define CLAMP01(val) CLAMP((val), 0.0, 1.0)

#endif // SHADER_CORE_GLSL