/**
 * @file maths_defines.h
 * 
 * @brief Common definitions and constants for maths.
 */

#ifndef MATHS_DEFINES_H
#define MATHS_DEFINES_H

#include "common/types.h"

// --- CONSTANTS --- //
/**
 * @brief Pi Constant (3.1415...)
 */
#define PI  3.1415926535897932384626433832795

/**
 * @brief Half Pi Constant
 */
#define PI_HALF ((PI) * 0.5)

/**
 * @brief Tau Constant (6.2831...)
 */
#define TAU 6.2831853071795864769252867665590

// --- MATH MACROS --- //

/**
 * @brief Get the absoule value of the val param
 * @param val value to get absolute
 */
#define ABS(val) ((val) > 0 ? (val) : -(val))

/**
 * @brief Truncate value
 * @param val value to truncate
 */
#define TRUNC(val) ((int)(val))

/**
 * @brief Find modulus value
 * @param a value to find remainder
 * @param b value to divide by
 */
#define MOD(a, b)  ((a) - (TRUNC((a) / (b)) * (b)))

/**
 * @brief Find the minimum between two values
 * @param a value
 * @param b value
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Find the maximum between two values
 * @param a value
 * @param b value
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Clamp a value between the min and max values
 * @param val value to clamp
 * @param min minimum value to clamp between
 * @param max maximum value to clamp between
 */
#define CLAMP(val, min, max) (MAX((min), MIN((max), (val))))

#define FLOOR(val) ((val) - (MOD(val, 1) >= 0 ? MOD(val, 1) : 1 + MOD(val, 1)))
#define CEIL(val)  (-FLOOR(-val))

#define TO_DEGREES(val) ((val) * (180.0 / PI))
#define TO_RADIANS(val) ((val) * (PI / 180.0))

#endif // MATHS_DEFINES_H
