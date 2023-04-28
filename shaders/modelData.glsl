#ifndef MODEL_DATA_GLSL
#define MODEL_DATA_GLSL

layout(set = 1, binding = 0) uniform UBModelData
{
// Matrices
  mat4 WVP;
  mat4 worldMat;
  mat4 invWorldMat;
} ModelData;

#endif // MODEL_DATA_GLSL