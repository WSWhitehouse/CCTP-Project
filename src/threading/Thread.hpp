#ifndef SNOWFLAKE_THREAD_HPP
#define SNOWFLAKE_THREAD_HPP

#include "pch.hpp"

// std includes
#include <functional>

namespace Threading
{
  // --- TYPEDEFS --- //

  /** @brief Thread Handle Type. */
  typedef void* Thread;

  /** @brief Thread ID Type. */
  typedef u64 ThreadID;

  /** @brief Function Ptr for starting a thread. */
  typedef std::function<void(void* data)> ThreadStartFunc;

  // --- THREAD MANAGEMENT --- //

  /**
  * @brief Starts a new thread with the provided function.
  * @param func Function to start on a new thread.
  * @param data Data passed to the function.
  * @param suspendOnStart Should the thread be suspended on start.
  * @return Handle to started thread on success; nullptr on failure.
  */
  Thread StartThread(ThreadStartFunc func, void* data, b8 suspendOnStart = false);

  /**
  * @brief Waits for the other thread to complete.
  * @param thread Thread to wait for.
  */
  void JoinThread(Thread thread);

  /**
  * @brief Suspends the thread.
  * @param thread Thread to suspend.
  * @return True on success; false otherwise.
  */
  b8 Suspend(Thread thread);

  /**
  * @brief Resumes the thread.
  * @param thread Thread to resume.
  * @return True on success; false otherwise.
  */
  b8 Resume(Thread thread);

  /**
  * @brief Get the handle for the current thread.
  * @return Current thread handle.
  */
  Thread GetCurrentThreadHandle();

  // --- THREAD ID --- //

  /**
  * @brief Get the ID of the current executing thread. The
  * ID is unique for as long as this thread is alive.
  * @return The thread ID.
  */
  ThreadID GetCurrentID();

  /**
  * @brief Get the ID of a thread object. The ID is unique
  * for as long as the thread is alive.
  * @param thread Thread handle to get the ID from.
  * @return The thread ID.
  */
  ThreadID GetID(Thread thread);


  // --- THREAD UTIL --- //

  /** @brief Returns the number of hardware threads available. */
  u32 GetHardwareThreadCount();

  /**
  * @brief Sleeps the current thread
  * @param ms Time in milliseconds to sleep for.
  */
  void Sleep(u64 ms);

} // namespace Threading

#endif //SNOWFLAKE_THREAD_HPP
