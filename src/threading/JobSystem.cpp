#include "threading/JobSystem.hpp"

// core
#include "core/Logging.hpp"

// threading
#include "threading/Thread.hpp"
#include "threading/sync/Latch.hpp"
#include "threading/sync/Flag.hpp"

// containers
#include <queue>

// std thread includes
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <utility>

// Forward Declarations
static void WorkerThreadRun(void* _index);

// Job Data
struct JobData
{
  Threading::Flag isComplete;
  JobSystem::WorkFuncPtr workFuncPtr;
};

static std::queue<JobData> jobQueue = {};

static std::mutex jobMutex           = {};
static std::condition_variable jobCV = {};

// Worker Threads
struct WorkerThread
{
  Threading::Thread thread;
  Threading::ThreadID id;
};

static constexpr const u64 minWorkerCount = 2;
static constexpr const u64 maxWorkerCount = U64_MAX - 1;

static WorkerThread* workerThreadPool = nullptr;
static u64 workerThreadCount          = 0;

// NOTE(WSWhitehouse): This is used in worker threads to identify
// themselves. Also used in IsWorkerThread() to quickly check if
// the calling thread is a worker thread from the Job System.
static thread_local u64 workerThreadIndex = U64_MAX;

// NOTE(WSWhitehouse): This latch ensures all worker threads are
// fully initialised before the manager can start issuing work.
static Threading::Latch workerThreadsInitLatch = {};

// System Variables
static std::atomic<bool> shutdownSystemFlag;

b8 JobSystem::Init()
{
  LOG_INFO("JobSystem: Initialisation Started...");

  shutdownSystemFlag.store(false, std::memory_order::seq_cst);

  // Calculating Worker Thread Count
  {
    const u64 hardwareThreads  = Threading::GetHardwareThreadCount();
    const u64 requestedThreads = MAX(hardwareThreads,  minWorkerCount);
    workerThreadCount          = MIN(requestedThreads, maxWorkerCount);

    LOG_INFO("JobSystem: Creating %u worker threads.", workerThreadCount);

    workerThreadsInitLatch.Init((i64)workerThreadCount);
  }

  // Worker Threads
  {
    workerThreadPool = (WorkerThread*) mem_alloc(sizeof(WorkerThread) * workerThreadCount);

    for (u32 i = 0; i < workerThreadCount; ++i)
    {
      WorkerThread& workerThread = workerThreadPool[i];

      // Start Thread
      u32* threadIndex = (u32*) mem_alloc(sizeof(u32));
      (*threadIndex)   = i;

      workerThread.thread = Threading::StartThread(WorkerThreadRun, threadIndex);
      workerThread.id     = Threading::GetID(workerThread.thread);

      LOG_DEBUG("JobSystem: Worker thread %u started (id: %u)", i, workerThread.id);
    }
  }

  // NOTE(WSWhitehouse): Wait for all worker threads to initialise...
//  workerThreadsInitLatch.Wait();

  LOG_INFO("JobSystem: Initialisation Complete!");
  return true;
}

void JobSystem::Shutdown()
{
  if (workerThreadPool == nullptr) return;

  LOG_INFO("JobSystem: Shutdown Started...");

  // Shutdown worker threads
  {
    shutdownSystemFlag.store(true, std::memory_order::seq_cst);
    jobCV.notify_all();

    LOG_INFO("JobSystem: Waiting for worker threads to finish...");
    for (u32 i = 0; i < workerThreadCount; ++i)
    {
      Threading::JoinThread(workerThreadPool[i].thread);
    }
  }

  // Free memory and reset variables
  {
    mem_free(workerThreadPool);
    workerThreadPool  = nullptr;
    workerThreadCount = 0;
  }

  LOG_INFO("JobSystem: Shutdown Complete!");
}

JobSystem::JobHandle JobSystem::SubmitJob(WorkFuncPtr workFuncPtr)
{
  JobHandle handle;

  // Add job to the queue
  {
    JobData jobData     = {};
    jobData.workFuncPtr = std::move(workFuncPtr);

    // Set up is complete flag...
    jobData.isComplete.Init();
    Threading::Flag::Future future = jobData.isComplete.GetFuture();
    handle.AssignJob(std::move(future));

    jobMutex.lock();
    jobQueue.emplace(std::move(jobData));
    jobMutex.unlock();
  }

  // Notify a worker
  jobCV.notify_one();

  return handle;
}

const u64& JobSystem::GetWorkerThreadCount() { return workerThreadCount; }
b8 JobSystem::IsWorkerThread() { return workerThreadIndex != U64_MAX; }

static void WorkerThreadRun(void* _index)
{
  workerThreadIndex = *((u32*)_index);
  mem_free(_index);

  LOG_DEBUG("Worker Thread %u Started", workerThreadIndex);
  workerThreadsInitLatch.CountDown();

  while(true)
  {
    std::unique_lock jobLock(jobMutex);
    jobCV.wait(jobLock, []{ return shutdownSystemFlag.load(std::memory_order::acquire) || !jobQueue.empty(); });

    // NOTE(WSWhitehouse): First check that the shutdown of worker threads has not been requested...
    if (shutdownSystemFlag.load(std::memory_order::acquire)) return;

    JobData currentJob = std::move(jobQueue.front());
    jobQueue.pop();

    const b8 isEmpty = jobQueue.empty();
    jobLock.unlock();

    if (!isEmpty) jobCV.notify_one();

    // Run job
    currentJob.workFuncPtr();
    currentJob.isComplete.Set();
  }
}
