#ifndef SNOWFLAKE_JOB_HANDLE_HPP
#define SNOWFLAKE_JOB_HANDLE_HPP

#include "pch.hpp"
#include "threading/sync/Flag.hpp"

namespace JobSystem
{
  struct JobHandle
  {
    JobHandle()  = default;
    ~JobHandle() = default;

    // Delete class copy
    JobHandle(const JobHandle& other)            = delete;
    JobHandle& operator=(const JobHandle& other) = delete;

    // Allow class move
    JobHandle(JobHandle&& other) noexcept            = default;
    JobHandle& operator=(JobHandle&& other) noexcept = default;

    /**
    * @brief Assign a job to this handle. Should only be called by the JobSystem!
    * @param completeFuture The future to associate with the job.
    */
    INLINE void AssignJob(Threading::Flag::Future&& completeFuture)
    {
      _completeFuture = std::move(completeFuture);
    }

    /**
    * @brief Waits until the job is complete, blocks the current thread!
    */
    INLINE void WaitUntilComplete()
    {
      return _completeFuture.Wait();
    }

    /**
    * @brief Check if the job is complete, should be non-blocking. But due
    * to the nature of multithreading and std::futures, this may not be the
    * case - if the thread does block, it should be for the smallest amount
    * of time!
    * @return True when complete; false otherwise.
    */
    [[nodiscard]] INLINE b8 IsComplete()
    {
      return _completeFuture.Get();
    }

  private:
    Threading::Flag::Future _completeFuture = {};
  };
}

#endif //SNOWFLAKE_JOB_HANDLE_HPP
