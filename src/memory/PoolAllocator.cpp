#include "PoolAllocator.hpp"

b8 PoolAllocator::Create(u64 size, u64 count)
{
  blockCount = count;
  memorySize = size * blockCount;

  // Allocate memory
  memoryStart = (byte*)mem_alloc(memorySize);
  memoryEnd   = memoryStart + memorySize;
  freeList    = (void**)mem_alloc(sizeof(void*) * blockCount);

  byte* ptr = memoryStart;
  for (u32 i = 0; i < blockCount; ++i)
  {
    freeList[i] = ptr;
    ptr += memorySize;
  }

  return true;
}

void PoolAllocator::Destroy()
{
  mem_free(memoryStart);
  mem_free(freeList);

  memorySize = 0;
  blockCount = 0;
  freeCount  = 0;
}
