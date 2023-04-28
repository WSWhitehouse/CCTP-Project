#ifndef UI_GLSL
#define UI_GLSL

/** @brief The output struct from the vertex shader */
struct VertOutput
{
  vec2 position;
  vec3 colour;
  vec2 texcoord;
};

/** @brief Canvas related data */
layout(set = 0, binding = 0) uniform UBUIData
{
  vec2 screenSize;
} UIData;

layout(push_constant) uniform PushConstData
{
  vec3 colour;
  vec2 pos;
  vec2 size;
  vec2 scale;
  float zOrder;
  float texIndex;
} imageData;

#define TARGET_SCREEN_WIDTH 1920
#define TARGET_SCREEN_HEIGHT 1080
#define TARGET_SCREEN_SIZE vec2(TARGET_SCREEN_WIDTH, TARGET_SCREEN_HEIGHT)

#endif // UI_GLSL