#ifndef SNOWFLAKE_LATCH_HPP
#define SNOWFLAKE_LATCH_HPP

#include "pch.hpp"

// std
#include <atomic>

namespace Threading
{

  /**
  * @brief A downward counter which can be used to synchronise threads. The counter
  * is initialised through the Init function, threads can count down using the
  * CountDown function. Threads can block and wait for the latch to release (counter
  * hits 0). The latch should be used as a single-use barrier.
  */
  struct Latch
  {
    /**
    * @brief Initialise the Latch to its initial count value.
    * @param expectedCount The value to initialise the latch to, must
    * be greater than or equal to 1, and less than I32_MAX.
    */
    INLINE void Init(i64 expectedCount)
    {
      expectedCount = CLAMP(expectedCount, 1, I32_MAX);
      count.store(expectedCount, std::memory_order::seq_cst);
      count.notify_all();
    }

    /**
    * @brief Count down the barrier.
    * @param updateCount Value to decrease count by.
    */
    INLINE void CountDown(i64 updateCount = 1)
    {
      const i64 old = count.fetch_sub(updateCount, std::memory_order::seq_cst);

      if (old == updateCount) count.notify_all();
    }

    /**
    * @brief Check if the latch is released. Does not block calling thread.
    * @return True when latch is released; false otherwise.
    */
    [[nodiscard]] INLINE b8 IsComplete() const
    {
      return count.load(std::memory_order::seq_cst) <= 0;
    }

    /**
    * @brief Blocks thread and waits for latch to be released.
    */
    INLINE void Wait() const
    {
      while (true)
      {
        const i64 old = count.load(std::memory_order::seq_cst);
        if (old <= 0) return;

        count.wait(old);
      }
    }

  private:
    std::atomic<i64> count = {};
  };

} // namespace Threading

#endif //SNOWFLAKE_LATCH_HPP
