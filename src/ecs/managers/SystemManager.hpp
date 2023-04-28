#ifndef SNOWFLAKE_SYSTEM_MANAGER_HPP
#define SNOWFLAKE_SYSTEM_MANAGER_HPP

#include "pch.hpp"

// std lib
#include <functional>

// Forward Declarations
//struct AppTime;
//struct StackAllocator;
//class Renderer;
//struct VkCommandBuffer;

namespace ECS
{

  class SystemManager
  {
     // --- FUNCTION PTR TYPEDEFS --- //
//     typedef std::function<void(const AppTime& appTime, StackAllocator& allocator)> UpdateFuncPtr;
//     typedef std::function<void(Renderer* renderer, VkCommandBuffer cmdBuffer)>     RenderFuncPtr;

    // TODO(WSWhitehouse): Create a system that will automatically manage calling system updates and render
    // functions, passing in the appropriate required parameters. This might not be 100% necessary as this
    // could just be done manually on a per system basis... but is probably needed for a entire game.
  };

} // namespace ECS

#endif //SNOWFLAKE_SYSTEM_MANAGER_HPP
