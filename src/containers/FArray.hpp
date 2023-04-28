#ifndef SNOWFLAKE_F_ARRAY_HPP
#define SNOWFLAKE_F_ARRAY_HPP

#include "pch.hpp"

/**
* @brief A templated fixed-size contiguous array. Similar semantics and uses as a C-style
* array, but doesn't decay to a pointer automatically. Also allows for fixed size arrays to
* be passed as function parameters more easily. This is a simpler version of the std::array
* type from the STL (and more consistent with the design of snowflake).
* @tparam Type Type of array to create.
* @tparam NumElements Number of elements in the array.
*/
template <typename Type, u64 NumElements>
struct FArray
{
  /** @brief The underlying array data. */
  Type data[NumElements];

  /** @brief Returns the number of elements in the array. */
  [[nodiscard]] INLINE constexpr u64 Size() const noexcept { return NumElements; }

  [[nodiscard]] INLINE constexpr const Type& operator[] (u64 index) const noexcept { return data[index]; }
  [[nodiscard]] INLINE constexpr       Type& operator[] (u64 index)       noexcept { return data[index]; }
};

#endif //SNOWFLAKE_F_ARRAY_HPP
