#ifndef SNOWFLAKE_COUNTER_HPP
#define SNOWFLAKE_COUNTER_HPP

/**
* @file Counter.hpp
* @brief A compile time preprocessor counter.
*
* HOW TO USE:
*   1. Create a new counter using `COUNTER_NEW(counter_name)`, replace counter_name
*      with a unique identifier for this counter.
*   2. Increment the counter by calling `COUNTER_INCREMENT(counter_name)`, replace
*      counter_name with the unique value used in step 1.
*   3. Read the value at any pont using `COUNTER_READ(counter_name)`.
*
* EXAMPLE:
*  @code
*   // Create a new counter named `temp`...
*   COUNTER_NEW(temp);
*
*   // Read the initial value...
*   const u32 a = COUNTER_READ(temp); // counter = 0
*
*   // Increment the counter a few times...
*   COUNTER_INCREMENT(temp);
*   COUNTER_INCREMENT(temp);
*   COUNTER_INCREMENT(temp);
*   COUNTER_INCREMENT(temp);
*   COUNTER_INCREMENT(temp);
*
*   // Read the counter...
*   const u32 b = COUNTER_READ(temp); // counter = 5
*
*   // Create a new separate counter...
*   COUNTER_NEW(temp2);
*   const u32 c = COUNTER_READ(temp2); // counter = 0
*   COUNTER_INCREMENT(temp2);
*   const u32 d = COUNTER_READ(temp2); // counter = 1
*
*   // Original counter remains in the same state...
*   const u32 e = COUNTER_READ(temp); // counter = 5
*   COUNTER_INCREMENT(temp);
*   const u32 f = COUNTER_READ(temp); // counter = 6
*  @endcode
*
* LINKS & SOURCES:
*   - https://stackoverflow.com/a/6174263/13195883
*   - https://ideone.com/yp19oo
*   - http://coliru.stacked-crooked.com/a/c7194d7c81f61a03
*/

#include "pch.hpp"

template<u64 n>
struct COUNTER_ConstIndex : std::integral_constant<u64, n> {};

template<typename counter, u64 rank, u64 acc>
consteval COUNTER_ConstIndex<acc> COUNTER_CounterCrumb(counter, COUNTER_ConstIndex<rank>, COUNTER_ConstIndex<acc>)
{ return {}; }

#define COUNTER_READ_CRUMB(counter, rank, acc) \
  COUNTER_CounterCrumb(counter(), COUNTER_ConstIndex<rank>(), COUNTER_ConstIndex<acc>())

#define COUNTER_NEW(counter) struct _COUNTER_##counter {}

#define COUNTER_READ(counter)                 \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 1,   \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 2,   \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 4,   \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 8,   \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 16,  \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 32,  \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 64,  \
  COUNTER_READ_CRUMB(_COUNTER_##counter, 128, \
    0))))))))

#define COUNTER_INCREMENT(counter)                                            \
  COUNTER_ConstIndex<COUNTER_READ(counter) + 1>                               \
  constexpr COUNTER_CounterCrumb(_COUNTER_##counter,                          \
    COUNTER_ConstIndex<(COUNTER_READ(counter) + 1) &~ COUNTER_READ(counter)>, \
    COUNTER_ConstIndex<(COUNTER_READ(counter) + 1) &  COUNTER_READ(counter)>) \
      { return {}; }

#endif //SNOWFLAKE_COUNTER_HPP
