/**
 * @file mem.h
 * 
 * @brief Interface for memory allocation and manipulation.
 */

#ifndef MEM_H
#define MEM_H

#include "common/types.h"
#include "common/defines.h"

/**
 * @brief Allocate new memory of a specific size. Zeros the memory after allocation
 * @param size size of memory block allocated
 * @return void* pointer to start of allocated memory
 */
void* mem_alloc(u64 size);

/**
 * @brief Reallocate memory of a specific size.
 * @param ptr pointer to memory block to reallocate.
 * @param newSize new size of memory block allocated
 * @return void* pointer to start of allocated memory
 */
void* mem_realloc(void* ptr, u64 newSize);

/**
 * @brief Frees memory that was previously allocated
 * @param ptr pointer to memory block to free
 */
void mem_free(void* ptr);

/**
 * @brief Sets the memory to a value
 * @param ptr pointer to memory block to set
 * @param value value to set the memory
 * @param size size of memory block to set
 */
void mem_set(void* ptr, i32 value, u64 size);

/**
 * @brief Sets the memory to zero
 * @param ptr pointer to memory block to set
 * @param size size of memory block to set
 */
static INLINE void mem_zero(void* ptr, u64 size) { mem_set(ptr, 0, size); }

/**
 * @brief Copies the data stored in a block of memory to another
 * @param dest Pointer to destination memory block
 * @param src Pointer to source memory block
 * @param size Size of memory block to copy
 */
void mem_copy(void* dest, const void* src, u64 size);

/**
 * @brief Moves the data stored in a block of memory to another
 * @param dest Pointer to destination memory block
 * @param src Pointer to source memory block
 * @param size Size of memory block to move
 */
void mem_move(void* dest, const void* src, u64 size);

#endif // MEM_H
