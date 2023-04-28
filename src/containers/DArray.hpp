#ifndef SNOWFLAKE_D_ARRAY_HPP
#define SNOWFLAKE_D_ARRAY_HPP

#include "pch.hpp"
#include "core/Logging.hpp"

#if defined(_DEBUG) || defined (_REL_DEBUG)

  #define DARRAY_VALID_CHECK()                                                                                  \
            do { if (!IsValid()) {                                                                              \
              LOG_FATAL("Trying to use an invalid DArray! Please call Create before calling this function..."); \
            } } while(false)

#else
  #define DARRAY_VALID_CHECK()
#endif

#define DARRAY_RESIZE_FACTOR 2

/**
* @brief A templated dynamic sized contiguous array. This is a simpler version of the std::vector
* type from the STL (and more consistent with the design of snowflake). Dynamic arrays aren't set
* up during its ctor, call the Create/Destroy functions appropriately to allocate/free the underlying
* array. Dynamic arrays resize when new elements are added if there isn't enough capacity - therefore,
* don't keep a pointer or reference to an element as it may be moved. Removing elements may result in
* some elements being moved to ensure the array continues to be contiguous.
* @tparam Type Type of array to create.
*/
template <typename Type>
struct DArray
{
  /** @brief The underlying array data. */
  Type* data = nullptr;

  [[nodiscard]] INLINE const Type& operator[] (u64 index) const noexcept { DARRAY_VALID_CHECK(); return data[index]; }
  [[nodiscard]] INLINE       Type& operator[] (u64 index)       noexcept { DARRAY_VALID_CHECK(); return data[index]; }

  /** @brief Get the number of elements in the array. */
  [[nodiscard]] INLINE u64 Size() const noexcept { return numElements; }

  /** @brief Get the capacity of the array, may not match Size(). */
  [[nodiscard]] INLINE u64 Capacity() const noexcept { return capacity; }

  /** @brief Returns if the DArray is valid. True when valid; false otherwise. */
  [[nodiscard]] INLINE b8 IsValid() const noexcept { return data != nullptr; }

  /**
  * @brief Create the dynamic array. Allocates the underlying memory and sets
  * the array to valid. See IsValid().
  * @param initialCapacity The initial capacity of the array. Default = 1.
  */
  INLINE void Create(u64 initialCapacity = 1)
  {
    // NOTE(WSWhitehouse): Recreating an already created DArray is valid behaviour,
    // so make sure the old array is cleaned up!
    if (IsValid()) { Destroy(); }

    const u64 initialActualCapacity = MAX(1, initialCapacity);

    data         = AllocDataArray(initialActualCapacity);
    numElements  = 0;
    capacity     = initialActualCapacity;
  }

  /**
  * @brief Destroy the array and free any underlying memory.
  * The array must be recreated before being used again.
  */
  INLINE void Destroy()
  {
    if (!IsValid()) return;

    mem_free(data);
    data        = nullptr;
    numElements = 0;
    capacity    = 0;
  }

  /**
  * @brief Resizes the array to the desired capacity. Will truncate and remove
  * elements already in the array if requested.
  * @param newCapacity Desired new capacity of array.
  * @param truncateArray Should elements be removed when resizing. Default = true.
  */
  INLINE void Resize(u64 newCapacity, b8 truncateArray = true)
  {
    DARRAY_VALID_CHECK();

    if (!truncateArray)
    {
      newCapacity = MAX(newCapacity, capacity);
    }

    if (newCapacity <= 0)
    {
      LOG_ERROR("Can't resize DArray to 0 capacity!");
      return;
    }

    u64 newElementCount = MIN(numElements, newCapacity);
    Type* newData = AllocDataArray(newCapacity);
    mem_copy(newData, data, sizeof(Type) * newElementCount);

    // Free old array
    mem_free(data);

    // Set new array values
    data        = newData;
    numElements = newElementCount;
    capacity    = newCapacity;
  }

  /** @brief Resizes the array to match the number of elements. */
  INLINE void ShrinkToNumElements()
  {
    Resize(1, false);
  }

  /**
  * @brief Add a new element to the array. May resize if there is not enough capacity.
  * @param element Element to add.
  * @return The index into the array where the element was added.
  */
  INLINE u64 Add(const Type& element)
  {
    DARRAY_VALID_CHECK();

    // NOTE(WSWhitehouse): Resize the array if we've hit capacity!
    if (numElements == capacity)
    {
      Resize(capacity * DARRAY_RESIZE_FACTOR, false);
    }

    const u64 elementIndex = numElements;
    numElements++;

    data[elementIndex] = element;
    return elementIndex;
  }

  /**
  * @brief Remove an element at the specified index.
  * @param index Element at index to remove.
  */
  INLINE void Remove(u64 index)
  {
    DARRAY_VALID_CHECK();

    if (index >= numElements)
    {
      LOG_ERROR("Trying to remove an element at an invalid index in a DArray!");
      return;
    }

    const u64 lastElementIndex = numElements - 1;

    // NOTE(WSWhitehouse): If the element being removed is the final element in the array
    // then there is no need to do anything other than reduce the number of elements.
    if (index == lastElementIndex)
    {
      numElements--;
      return;
    }

    data[lastElementIndex].~Type();

    // Copy value from end of array to fill the gap...
    mem_copy(&data[index], &data[lastElementIndex], sizeof(Type));
    numElements--;
  }

private:
  u64 numElements  = 0;
  u64 capacity     = 0;

  /**
  * @brief Helper function to allocate memory for the data array at the specified capacity.
  * @param capacity Capacity to allocate.
  * @return Pointer to allocated memory.
  */
  static INLINE Type* AllocDataArray(u64 capacity)
  {
    return (Type*)mem_alloc(sizeof(Type) * capacity);
  }
};

// NOTE(WSWhitehouse): Shouldn't use this macro outside of this file...
#undef DARRAY_VALID_CHECK

#endif //SNOWFLAKE_D_ARRAY_HPP
