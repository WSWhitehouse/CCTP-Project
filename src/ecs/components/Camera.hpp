#ifndef SNOWFLAKE_CAMERA_HPP
#define SNOWFLAKE_CAMERA_HPP

#include "pch.hpp"

struct Camera
{
  // Matrices
  glm::mat4 viewMatrix;
  glm::mat4 projMatrix;
  glm::mat4 inverseViewMatrix;
  glm::mat4 inverseProjMatrix;

  // Data
  f32 fovY          = glm::radians(60.0f);
  f32 nearClipPlane = 0.1f;
  f32 farClipPlane  = 200.0f;
  glm::vec3 upAxis  = glm::vec3(0.0f, -1.0f, 0.0f);
};


#endif //SNOWFLAKE_CAMERA_HPP
