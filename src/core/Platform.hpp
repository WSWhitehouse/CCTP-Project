#ifndef SNOWFLAKE_PLATFORM_HPP
#define SNOWFLAKE_PLATFORM_HPP

#include "pch.hpp"

// REVIEW(WSWhitehouse): Rethink this. I'm not a fan of a dedicated platform class
// like this, it required an init and shutdown that do barely anything. Would be
// best if this just works and its direct platform function calls behind the scene.

// TODO(WSWhitehouse): Add multithreading support.

namespace Platform
{

  b8 Init();
  void Shutdown();

  f64 GetTime();

  void* GetNativeInstance();

} // namespace Platform



#endif //SNOWFLAKE_PLATFORM_HPP
