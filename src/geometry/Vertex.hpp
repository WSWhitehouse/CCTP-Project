#ifndef SNOWFLAKE_VERTEX_HPP
#define SNOWFLAKE_VERTEX_HPP

#include "pch.hpp"

// containers
#include "containers/FArray.hpp"

#include <vulkan/vulkan.h>

// Forward Declarations
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

struct Vertex
{
  Vertex()
  {
    position = glm::vec3(0.0f, 0.0f, 0.0f);
    normal   = glm::vec3(0.0f, 0.0f, 0.0f);
    colour   = glm::vec3(1.0f, 1.0f, 1.0f);
    texcoord = glm::vec2(0.0f, 0.0f);
  }

  constexpr Vertex(glm::vec3 pos, glm::vec2 uv, glm::vec3 normals,
                   glm::vec3 col = glm::vec3(1.0f, 1.0f, 1.0f))
    : position(pos), normal(normals), colour(col), texcoord(uv)
  { }

  DEFAULT_CLASS_COPY(Vertex);

  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 normal;
  alignas(16) glm::vec3 colour;
  alignas(8)  glm::vec2 texcoord;

  // --- VULKAN UTIL FUNCTIONS --- //
  static constexpr VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static constexpr FArray<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
  {
    FArray<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};

    // Position
    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = offsetof(Vertex, position);

    // Position
    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset   = offsetof(Vertex, normal);

    // Colour
    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset   = offsetof(Vertex, colour);

    // Texcoord
    attributeDescriptions[3].binding  = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset   = offsetof(Vertex, texcoord);

    return attributeDescriptions;
  }

  // NOTE(WSWhitehouse): Using auto here so if/when the attribute descriptions array
  // size changes this variable doesn't need to be updated with it...
  static inline const auto BindingDescription    = Vertex::GetBindingDescription();
  static inline const auto AttributeDescriptions = Vertex::GetAttributeDescriptions();
};

#endif //SNOWFLAKE_VERTEX_HPP
