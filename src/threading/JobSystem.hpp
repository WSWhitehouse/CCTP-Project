#ifndef SNOWFLAKE_JOB_SYSTEM_HPP
#define SNOWFLAKE_JOB_SYSTEM_HPP

#include "pch.hpp"
#include "threading/JobHandle.hpp"

#include <functional>

namespace JobSystem
{
  /**
  * @brief The function pointer typedef for submitting
  * work to the job system. Must externally synchronise
  * user data.
  */
  typedef std::function<void()> WorkFuncPtr;

  /**
  * @brief Initialise the Job System. Must be called before
  * submitting any work!
  * @return True on success; false otherwise.
  */
  b8 Init();

  /**
  * @brief Shutdown the Job System. Ensures all worker threads
  * are joined - this means waiting for any jobs to finish on
  * other worker threads.
  */
  void Shutdown();

  /**
  * @brief Submit work to be completed by the job system. Any
  * user data *MUST* be externally synchronised!
  * @param workFuncPtr Function Ptr to work function.
  * @return A JobHandle to the submitted job.
  */
  JobHandle SubmitJob(WorkFuncPtr workFuncPtr);

  /** @brief Get the number of worker threads in the job system. */
  [[nodiscard]] const u64& GetWorkerThreadCount();

  /**
  * @brief Check if the current executing thread is a worker thread.
  * @return True if thread is a worker thread; false otherwise.
  */
  [[nodiscard]] b8 IsWorkerThread();

} // namespace JobSystem


#endif //SNOWFLAKE_JOB_SYSTEM_HPP
