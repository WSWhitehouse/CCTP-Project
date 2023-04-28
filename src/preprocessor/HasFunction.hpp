#ifndef SNOWFLAKE_HAS_FUNCTION_HPP
#define SNOWFLAKE_HAS_FUNCTION_HPP

/**
 * @file HasFunction.hpp
 * @brief A compile time macro for detecting if a function returning void exists in a class/struct.
 *
 * HOW TO USE:
 *   - Define the function check in the C++ file: DEFINE_HAS_FUNCTION_CHECK(funcName, function params).
 *   - Use the HAS_FUNCTION macro to check if the type contains the function.
 *
 * EXAMPLE:
 * @code
 * // Define the initial check for a function called "FuncName" with two parameters (i32 and f32)
 * DEFINE_HAS_FUNCTION_CHECK(FuncName, i32, f32);
 *
 * // Check if "TestClass" has "FuncName"
 * b8 hasFunction = HAS_FUNCTION(TestClass, FuncName);
 * @endcode
 *
 * LINKS & SOURCES:
 *   - https://retroscience.net/cpp-detect-functions-template.html
 *   - https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
 */

#include "pch.hpp"
#include "preprocessor/CommaListArgs.hpp"

#define DECLVAL_ARG(x) std::declval<x>()

#define DEFINE_HAS_FUNCTION_CHECK(funcName, ...)             \
  template<typename T> struct Has##funcName##Function        \
  {                                                          \
   private:                                                  \
    template<typename>                                       \
    static consteval std::false_type                         \
    HasCheck(...) noexcept { return std::false_type(); }     \
                                                             \
    template<typename U>                                     \
    static consteval decltype(std::declval<U>().funcName(    \
        COMMA_LIST(DECLVAL_ARG, VA_ARGS_EXPAND(__VA_ARGS__)) \
        ), std::true_type()                                  \
    ) HasCheck(i32) noexcept { return std::true_type(); }    \
                                                             \
   public:                                                   \
    static constexpr b8 value =                              \
      std::is_same<decltype(HasCheck<T>(0)),                 \
                   std::true_type>::value;                   \
  }

#define HAS_FUNCTION(type, funcName) Has##funcName##Function<type>::value

#endif //SNOWFLAKE_HAS_FUNCTION_HPP
