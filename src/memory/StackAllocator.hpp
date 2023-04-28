#ifndef SNOWFLAKE_STACK_ALLOCATOR_HPP
#define SNOWFLAKE_STACK_ALLOCATOR_HPP

#include "pch.hpp"
#include "core/Abort.hpp"
#include "core/Assert.hpp"

class StackAllocator
{
  DELETE_CLASS_COPY(StackAllocator);

public:
  StackAllocator()  = default;
  ~StackAllocator() = default;

  /**
  * @brief The type used by the Stack Allocator to mark a position
  * on the stack. Can be acquired using the GetMarker() function.
  * The stack can be rolled back to a marker using FreeToMarker().
  */
  typedef byte* Marker;

  /**
  * @brief CreateECS a new Stack Allocator, using a constructor to
  * stop any user code from reallocating the initial pool or
  * destroying the allocator.
  * @param totalSize Size for the allocator.
  */
  INLINE b8 Create(u32 _totalSize)
  {
    memorySize   = _totalSize;
    memoryStart = (byte*)mem_alloc(memorySize);
    memoryEnd   = memoryStart + memorySize;
    current     = memoryStart;

    mem_zero(memoryStart, memorySize);
    return true;
  }

  /** @brief Destroy the stack allocator and free the memory. */
  INLINE void Destroy()
  {
    // Free actual memory block
    mem_free(memoryStart);

    // Reset all values
    memorySize   = 0;
    memoryStart = nullptr;
    memoryEnd   = nullptr;
    current     = nullptr;
  }

  /**
  * @brief Allocate memory from stack allocator.
  * @param size Size of memory to allocate.
  * @return Pointer to allocated memory.
  */
  INLINE void* Allocate(u32 size)
  {
    ASSERT_MSG(memoryStart != nullptr, "Memory is nullptr! Have you called CreateECS()?");

    void* ptr = current;
    current += size;

#if _DEBUG
    // NOTE(WSWhitehouse): Check that the current ptr isn't after
    // the end of the memory block, we don't want to use memory
    // this stack allocator doesn't own.
    if (current >= memoryEnd)
    {
      LOG_FATAL("Stack Allocator: Allocating more memory than total size (alloc size: %i, total size: %i)", size, memorySize);
      ABORT(ABORT_CODE_MEMORY_ALLOC_FAILURE);
    }
#endif

    return ptr;
  }

  /**
  * @brief Free all memory in stack allocator. WARNING: All previous
  * allocations are invalid and those pointers should not be used!
  * @param zeroMemory Should the memory be set to 0 on reset? This is
  * related to memory security - perhaps the memory in this region
  * should not be accessed after this free.
  */
  INLINE void FreeAll(const bool zeroMemory = false)
  {
    ASSERT_MSG(memoryStart != nullptr, "Memory is nullptr! Have you called CreateECS()?");
    current = memoryStart;

    if (zeroMemory) { mem_zero(memoryStart, memorySize); }
  }

  /**
  * @brief Get the current marker in the stack. Can be used
  * with FreeToMarker to rollback the stack to this position.
  * @return Marker at current position in the stack allocator.
  */
  [[nodiscard]] INLINE Marker GetMarker() const
  {
    ASSERT_MSG(memoryStart != nullptr, "Memory is nullptr! Have you called CreateECS()?");
    return (Marker)current;
  }

  /**
  * @brief Rollback the stack to the marker. WARNING: All allocations
  * made after the Marker was acquired are invalid and should not be used!
  * @param marker Marker to rollback too.
  */
  INLINE void FreeToMarker(Marker marker)
  {
    ASSERT_MSG(memoryStart != nullptr, "Memory is nullptr! Have you called CreateECS()?");
    current = (byte*)marker;
  }

private:
  u32 memorySize    = 0;
  byte* memoryStart = nullptr;
  byte* memoryEnd   = nullptr;
  byte* current     = nullptr;
};

#endif //SNOWFLAKE_STACK_ALLOCATOR_HPP
