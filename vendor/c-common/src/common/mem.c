#include "common.h"
#include <stdlib.h> // malloc, free
#include <string.h> // memcpy, memset, memmove

void* mem_alloc(u64 size)
{
  void* ptr = malloc(size);
  mem_zero(ptr, size);
  return ptr;
}

void* mem_realloc(void* ptr, u64 newSize)
{
  return realloc(ptr, newSize);
}

void mem_free(void* ptr)
{
  free(ptr);
}

void mem_set(void* ptr, i32 value, u64 size)
{
  memset(ptr, value, size);
}

void mem_copy(void* dest, const void* src, u64 size)
{
  memcpy(dest, src, size);
}

void mem_move(void* dest, const void* src, u64 size)
{
  memmove(dest, src, size);
}
