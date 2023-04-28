#ifndef SNOWFLAKE_PCH_HPP
#define SNOWFLAKE_PCH_HPP

// --- Common --- //
#include <common.h>

// Standard Lib
#include <iostream>

// Containers
#include <vector>

// --- GLM --- //
#define GLM_FORCE_SILENT_WARNINGS    // remove unnecessary warnings
#define GLM_FORCE_RADIANS            // always work with radians
#define GLM_FORCE_XYZW_ONLY          // forces xyzw in vecs and quat to help debugging
#define GLM_FORCE_QUAT_DATA_XYZW     // forces quat to be in xyzw order
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // sets depth to 0 to 1
#include <glm.hpp>
#include <ext.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/transform.hpp>
#include <gtx/quaternion.hpp>

// --- DEFINITIONS --- //

/**
* @brief Swap two values.
* @param type The type of the two values.
* @param lhs The first argument to swap.
* @param rhs The second argument to swap.
*/
#define SWAP(type, lhs, rhs)                  \
do {                                          \
  STATIC_ASSERT(sizeof(lhs) == sizeof(type)); \
  STATIC_ASSERT(sizeof(rhs) == sizeof(type)); \
                                              \
  type temp;                                  \
  mem_move(&temp, &(lhs), sizeof(type));      \
                                              \
  mem_move(&(lhs), &(rhs), sizeof(type));     \
  mem_move(&(rhs), &temp, sizeof(type));      \
} while (false)

/**
* @brief Define this macro in the body of a class to
* declare the class as static. Deletes constructors,
* destructors, and copy-assignment operators.
*/
#define DECLARE_STATIC_CLASS(T)                \
  public:                                      \
     T() = delete;                             \
    ~T() = delete;                             \
     DELETE_CLASS_COPY(T)

/**
* @brief Deletes copy constructors and
* copy-assignment operators from a class.
*/
#define DELETE_CLASS_COPY(T)                   \
  public:                                      \
    T(const T& other)                = delete; \
    T(T&& other) noexcept            = delete; \
    T& operator=(const T& other)     = delete; \
    T& operator=(T&& other) noexcept = delete;

/**
* @brief Defaults copy constructors and
* copy-assignment operators for a class.
*/
#define DEFAULT_CLASS_COPY(T)                   \
  public:                                       \
    T(const T& other)                = default; \
    T(T&& other) noexcept            = default; \
    T& operator=(const T& other)     = default; \
    T& operator=(T&& other) noexcept = default;

#endif //SNOWFLAKE_PCH_HPP
