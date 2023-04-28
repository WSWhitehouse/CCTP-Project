#include "geometry/Vertex.hpp"
#include <vulkan/vulkan.h>

//constexpr VkVertexInputBindingDescription Vertex::GetBindingDescription()
//{
//  VkVertexInputBindingDescription bindingDescription = {};
//  bindingDescription.binding   = 0;
//  bindingDescription.stride    = sizeof(Vertex);
//  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//  return bindingDescription;
//}
//
//constexpr FArray<VkVertexInputAttributeDescription, 4> Vertex::GetAttributeDescriptions()
//{
//  FArray<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};
//
//  // Position
//  attributeDescriptions[0].binding  = 0;
//  attributeDescriptions[0].location = 0;
//  attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
//  attributeDescriptions[0].offset   = offsetof(Vertex, position);
//
//  // Position
//  attributeDescriptions[1].binding  = 0;
//  attributeDescriptions[1].location = 1;
//  attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
//  attributeDescriptions[1].offset   = offsetof(Vertex, normal);
//
//  // Colour
//  attributeDescriptions[2].binding  = 0;
//  attributeDescriptions[2].location = 2;
//  attributeDescriptions[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
//  attributeDescriptions[2].offset   = offsetof(Vertex, colour);
//
//  // Texcoord
//  attributeDescriptions[3].binding  = 0;
//  attributeDescriptions[3].location = 3;
//  attributeDescriptions[3].format   = VK_FORMAT_R32G32_SFLOAT;
//  attributeDescriptions[3].offset   = offsetof(Vertex, texcoord);
//
//  return attributeDescriptions;
//}