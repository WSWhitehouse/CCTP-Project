#ifndef SNOWFLAKE_TRANSFORM_HPP
#define SNOWFLAKE_TRANSFORM_HPP

#include "pch.hpp"
#include "Camera.hpp"
#include "imgui.h"
#include "math/Math.hpp"

struct Transform
{
  glm::vec3 position = { 0.0f, 0.0f, 0.0f };
  glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; // euler angles
  glm::vec3 scale    = { 1.0f, 1.0f, 1.0f };

  glm::mat4x4 matrix = glm::mat4x4(1.0f);

  // --- UTILITY FUNCTIONS --- //

  [[nodiscard]]
  INLINE glm::mat4x4 GetWVPMatrix(const Camera& camera) const
  {
    return camera.projMatrix * camera.viewMatrix * matrix;
  }

  [[nodiscard]]
  INLINE glm::mat4x4 CreateTRSMatrix() const
  {
    return Math::CreateTRSMatrix(position, rotation, scale);
  }

  [[nodiscard]]
  INLINE glm::vec3 GetForwardDir() const
  {
    return { glm::sin(rotation.y), -rotation.x, glm::cos(rotation.y) };
  }

  INLINE void DrawGUI(const char* title)
  {
    ImGui::Begin(title);
    ImGui::InputFloat3("position",  glm::value_ptr(position));
    ImGui::SliderFloat3("rotation", glm::value_ptr(rotation), 0.0f, 360.0f);
    ImGui::InputFloat3("scale",     glm::value_ptr(scale));
    ImGui::End();
  }
};

#endif //SNOWFLAKE_TRANSFORM_HPP
