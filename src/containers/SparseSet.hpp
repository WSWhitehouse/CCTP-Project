#ifndef SNOWFLAKE_SPARSE_SET_HPP
#define SNOWFLAKE_SPARSE_SET_HPP

#include "pch.hpp"
#include "core/Logging.hpp"

/**
* @brief A sparse set is a data structure that allows for tightly packed
* arrays of items. With a sparse array that maps the sets items to their
* indices in the sparse array.
*
* Check out these links for more information:
*   - https://manenko.com/2021/05/23/sparse-sets.html
*   - https://gitlab.com/manenko/rho-sparse-set/-/blob/master/include/rho/sparse_set.h
*   - https://www.codeproject.com/Articles/859324/Fast-Implementations-of-Sparse-Sets-in-Cplusplus
*/
template<typename Type>
struct SparseSet
{
  struct DenseType
  {
    u32 sparseIndex;
    Type type;
  };

  DenseType* denseArray  = nullptr;
  u32* sparseArray       = nullptr;

  u32 denseCount = 0;
  u32 maxCount   = 0;

  /**
  * @brief Initialise the Sparse Set.
  * @param count Number of elements in the set.
  */
  INLINE void Init(u32 count)
  {
    denseCount = 0;
    maxCount   = count;

    const u64 arraySize = sizeof(DenseType) * maxCount;
    denseArray  = (DenseType*)mem_alloc(arraySize);
    sparseArray = (u32*)mem_alloc(arraySize);
  }

  /**
  * @brief Destroys the Sparse Set and frees its memory.
  */
  INLINE void Destroy()
  {
    denseCount = 0;
    maxCount   = 0;

    mem_free(denseArray);
    mem_free(sparseArray);
    denseArray  = nullptr;
    sparseArray = nullptr;
  }

  /**
  * @brief Add an item to the SparseSet.
  * @param item Item to add.
  */
  INLINE void Add(u32 item)
  {
    if (item >= maxCount)
    {
      LOG_ERROR("SparseSet: id (%u) is greater than the maximum count!", item);
      return;
    }

    if (Contains(item)) return;

    denseArray[denseCount] = item;
    sparseArray[item]      = denseCount;

    denseCount++;
  }

  /**
  * @brief Remove an item from the SparseSet. Does not perform a
  * HasComponent check. Removing an item not in the set is undefined
  * behaviour!
  * @param index Index to remove.
  * @return Dense array index where the item was removed from.
  */
  INLINE u32 Remove(u32 index)
  {
    denseCount--;

    // Move the last item in the dense array into the empty
    // slot where the item we want to remove is...
    const u32 lastDenseIndex = denseCount;
    const u32 denseIndex     = sparseArray[index];

    mem_copy(&denseArray[denseIndex], &denseArray[lastDenseIndex], sizeof(DenseType));
    sparseArray[denseArray[denseIndex].sparseIndex] = denseIndex;
    return denseIndex;
  }

  /**
  * @brief Check if the SparseSet contains an item.
  * @param index Index to check for.
  * @return True if the SparseSet contains the item; false otherwise.
  */
  [[nodiscard]] INLINE b8 Contains(u32 index) const
  {
    const u32& denseIndex = sparseArray[index];
    if (denseIndex >= denseCount) return false;

    return denseArray[denseIndex].sparseIndex == index;
  }

  /**
  * @brief Clear the SparseSet.
  */
  INLINE void Clear()
  {
    denseCount = 0;
  }

  [[nodiscard]] INLINE const Type& operator[] (u32 index) const noexcept
  {
    const u32& denseIndex = sparseArray[index];
    return &denseArray[denseIndex].type;
  }

  [[nodiscard]] INLINE Type& operator[] (u32 index) noexcept
  {
    const u32& denseIndex = sparseArray[index];
    return &denseArray[denseIndex].type;
  }

};

#endif //SNOWFLAKE_SPARSE_SET_HPP
