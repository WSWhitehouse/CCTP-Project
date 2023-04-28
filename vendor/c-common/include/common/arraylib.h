/**
 * @file arraylib.h
 * 
 * @brief Provides common utilities for arrays.
 */

#ifndef ARRAYLIB_H
#define ARRAYLIB_H

// NOTE(WSWhitehouse): Adding the "no lint" comment on ARRAY_SIZE macro stops
// clang-tidy complaining about unusual array subscript. But this is required
// for the macro to work correctly.

/** @brief Calculate the size of an array */
#define ARRAY_SIZE(arr) ((sizeof(arr) / sizeof(0[arr])) / ((size_t)(!(sizeof(arr) % sizeof(0[arr]))))) /* NOLINT */

/**
 * @brief Wraps index based on array size. Does not support negative numbers.
 * @param arr_size size of array
 * @param arr_index index to wrap
 */
#define ARRAY_WRAP_INDEX(arr_size, arr_index) (((arr_index) + (arr_size)) % (arr_size))

/**
 * @brief Wraps index based on array size. This is safe for negative
 * numbers, but requires an extra modulus - so is *slightly* slower.
 * @param arr_size size of array
 * @param arr_index index to wrap
 */
#define ARRAY_WRAP_INDEX_SAFE(arr_size, arr_index) ((((arr_index) % (arr_size)) + (arr_size)) % (arr_size))

#endif // ARRAYLIB_H
