#ifndef SNOWFLAKE_ENTITY_HPP
#define SNOWFLAKE_ENTITY_HPP

#include "pch.hpp"
#include "core/Assert.hpp"

namespace ECS
{
  typedef u32 Entity;

  // --- CONST DEFINITIONS --- //
  static inline constexpr const Entity NULL_ENTITY      = U32_MAX;
  static inline constexpr const u32 MAX_ENTITY_COUNT    = 5000;

  // --- STATIC ASSERTS --- //
  STATIC_ASSERT(MAX_ENTITY_COUNT < U32_MAX, "Cannot support more than U32_MAX entities!");

} // namespace ECS

#endif //SNOWFLAKE_ENTITY_HPP
