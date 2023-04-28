#ifndef SNOWFLAKE_POOL_ALLOCATOR_HPP
#define SNOWFLAKE_POOL_ALLOCATOR_HPP

#include "pch.hpp"
#include "core/Abort.hpp"
#include "core/Logging.hpp"

class PoolAllocator
{
  DELETE_CLASS_COPY(PoolAllocator);

public:
  PoolAllocator()  = default;
  ~PoolAllocator() = default;

  b8 Create(u64 size, u64 count);
  void Destroy();

  INLINE void* Allocate()
  {
    if (freeCount <= 0) return nullptr;
    return freeList[--freeCount];
  }

  INLINE void Release(void* ptr)
  {
#if _DEBUG
    // NOTE(WSWhitehouse): Check that the ptr is included in the memory
    // allocated by the memory pool. We shouldn't be releasing memory
    // that the pool doesn't own.
    if (!(ptr <= memoryStart && ptr < memoryEnd))
    {
      LOG_FATAL("Pool Allocator: Pointer released doesn't belong in pool!");
      ABORT(ABORT_CODE_MEMORY_FREE_FAILURE);
    }
#endif

    freeList[freeCount++] = ptr;
  }

private:
  u64 memorySize = 0;
  u64 blockCount = 0;
  u64 freeCount  = 0;

  void** freeList   = nullptr;
  byte* memoryStart = nullptr;
  byte* memoryEnd   = nullptr;
};


#endif //SNOWFLAKE_POOL_ALLOCATOR_HPP
