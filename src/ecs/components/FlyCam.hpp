#ifndef SNOWFLAKE_FLY_CAM_HPP
#define SNOWFLAKE_FLY_CAM_HPP

#include "pch.hpp"

struct FlyCam
{
  f32 moveSpeed       = 10.0f;
  glm::vec2 lookSpeed = {1.0f, 3.0f};

  glm::vec2 prevMousePos;
};

#endif //SNOWFLAKE_FLY_CAM_HPP
