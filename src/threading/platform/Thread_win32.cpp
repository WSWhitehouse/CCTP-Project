#include "pch.hpp"

#if defined(PLATFORM_WINDOWS)

#include "threading/Thread.hpp"
#include <windows.h>
#include <future>
#include <utility>

using namespace Threading;

struct ThreadProcData
{
  ThreadStartFunc funcPtr;
  void* data;
  b8 suspend;

  std::promise<void> funcStarted;
};

static DWORD WINAPI ThreadProc(void* data)
{
  ThreadProcData* procData = (ThreadProcData*)data;

  ThreadStartFunc funcPtr = std::move(procData->funcPtr);
  void* funcData          = std::move(procData->data);
  b8 suspend              = std::move(procData->suspend);

  // NOTE(WSWhitehouse): Should not access procData after setting the promise value as it should not
  // be considered valid after this point as it has probably gone out of scope (see Thread::Start)...
  procData->funcStarted.set_value();

  if (suspend) Threading::Suspend(Threading::GetCurrentThreadHandle());

  funcPtr(funcData);
  return 0;
}

Thread Threading::StartThread(ThreadStartFunc func, void* data, b8 suspendOnStart)
{
  // NOTE(WSWhitehouse): Unfortunately, we have to do all this nonsense when creating a thead as
  // the Win32 CreateThread call doesn't take in a function that returns void. This ensures we
  // don't allocate any additional memory, but wait for the current thread to start processing
  // before moving on. This does mean starting threads can cause a wait.

  ThreadProcData procData = {};
  procData.funcPtr        = std::move(func);
  procData.data           = data;
  procData.suspend        = suspendOnStart;
  procData.funcStarted    = {};

  std::future<void> threadStarted = procData.funcStarted.get_future();

  HANDLE thread = ::CreateThread(nullptr, 0, ThreadProc, &procData, 0, nullptr);

  // NOTE(WSWhitehouse): Need to check if CreateThread failed, as
  // this will cause a deadlock on waiting for the future below.
  if (thread == nullptr) return nullptr;

  threadStarted.get();
  return (Thread)thread;
}

void Threading::JoinThread(Thread thread)
{
  ::WaitForSingleObject((HANDLE)thread, INFINITE);
}

b8 Threading::Suspend(Thread thread)
{
  DWORD result = ::SuspendThread((HANDLE)thread);
  if (result == ((DWORD)-1)) return false;

  return true;
}

b8 Threading::Resume(Thread thread)
{
  DWORD result = ::ResumeThread((HANDLE)thread);
  if (result == ((DWORD)-1)) return false;

  return true;
}

Thread Threading::GetCurrentThreadHandle()
{
  return ::GetCurrentThread();
}

ThreadID Threading::GetCurrentID()
{
  return GetID(GetCurrentThreadHandle());
}

ThreadID Threading::GetID(Thread thread)
{
  return (ThreadID)::GetThreadId((HANDLE)thread);
}

u32 Threading::GetHardwareThreadCount()
{
  SYSTEM_INFO info; GetSystemInfo(&info);
  return (u32)info.dwNumberOfProcessors;
}

void Threading::Sleep(u64 ms)
{
  ::Sleep(ms);
}

#endif // defined(PLATFORM_WINDOWS)