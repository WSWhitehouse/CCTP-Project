#include "Logging.hpp"

// std includes
#include <cstdlib>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

// Forward Declarations
struct LogEntry;
static void LoggingThreadRun();
static INLINE void PrintTime(const std::time_t& time);
static INLINE void PrintLogEntry(const LogEntry& logEntry);

static const char* LevelStrings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[PROF]:  "};

/** @brief The max length a message can be. */
#define MSG_LENGTH 32000

/** @brief The max number of logs the queues can support. */
#define MAX_LOGS 10000

// NOTE(WSWhitehouse): Used on the logging thread to output messages to the console, this is
// only accessed on the main logging thread, so does not need synchronisation.
static char outMsg[MSG_LENGTH];

struct LogEntry
{
  std::time_t time;
  Logging::LogLevel level;
  const char msg[MSG_LENGTH] = {0};
};

struct LogQueue
{
  LogEntry queue[MAX_LOGS] = {};
  u32 entryCount           = 0;
};

static std::mutex queueMutex;
static LogQueue logQueue;

// NOTE(WSWhitehouse): This is the local copy of the log queue, it is only accessed on the main
// logging thread. That thread copies the main queue, resets and releases the lock to allow other
// threads to add logs while they are printed to the console.
static LogQueue localLogQueue;

static std::thread loggingThread;
static std::condition_variable loggingCV;
static b8 loggingThreadShouldExit = true;

b8 Logging::Init()
{
  loggingThreadShouldExit = false;
  loggingThread = std::thread(LoggingThreadRun);

  // NOTE(WSWhitehouse): Ensure the logger is shutdown correctly on exit, this allows any log
  // entries in the queue to be flushed to the console...
  std::atexit(Logging::Shutdown);

  LOG_INFO("Logging Successfully Initialised!");
  return true;
}

void Logging::Shutdown()
{
  // NOTE(WSWhitehouse): Logging is already in the process of shutting down, ignore this request...
  if (loggingThreadShouldExit) return;

  LOG_INFO("Logging Shutting Down...");

  // Signal the logging thread to exit...
  queueMutex.lock();
  loggingThreadShouldExit = true;
  queueMutex.unlock();

  // Wait for logging thread to finish...
  loggingCV.notify_all();
  loggingThread.join();
}

void Logging::LogMessage(Logging::LogLevel level, const char* msg, ...)
{
  // Get time of message
  auto timeNow    = std::chrono::system_clock::now();
  std::time_t now = std::chrono::system_clock::to_time_t(timeNow);

  // Construct log entry
  LogEntry logEntry{ .time = now, .level = level };

  // Format original message
  {
    #if defined(COMPILER_MSC)
        va_list arg_ptr;
    #else
        __builtin_va_list arg_ptr;
    #endif

    // Ensure message is clear
    mem_zero((char*)(logEntry.msg), MSG_LENGTH);

    va_start(arg_ptr, msg);
    vsnprintf((char*)(logEntry.msg), MSG_LENGTH, msg, arg_ptr);
    va_end(arg_ptr);
  }

  queueMutex.lock();

  // NOTE(WSWhitehouse): Technically doing a lot of work here to add a message full
  // entry. Hopefully, this won't be hit often (or at all) during runtime if the
  // MAX_LOGS is set to something appropriate. The queue message will overwrite
  // whatever the final message entry is (including another copy of the full queue
  // message).
  if (logQueue.entryCount >= MAX_LOGS)
  {
    const char* MaxLogsStr = "LOGGING MESSAGE QUEUE FULL! Consider increasing MAX_LOGS!";

    mem_zero((void*) &logEntry.msg, MSG_LENGTH);
    snprintf((char*) &logEntry.msg, MSG_LENGTH, "%s", MaxLogsStr);

    mem_copy(&logQueue.queue[MAX_LOGS - 1], &logEntry, sizeof(LogEntry));
    queueMutex.unlock();

    loggingCV.notify_one();
    return;
  }

  mem_copy(&logQueue.queue[logQueue.entryCount], &logEntry, sizeof(LogEntry));
  logQueue.entryCount++;
  queueMutex.unlock();

  loggingCV.notify_one();
}

void Logging::LogMessageImmediate(const Logging::LogLevel level, const char* msg, ...)
{
  // Get time of message
  auto timeNow    = std::chrono::system_clock::now();
  std::time_t now = std::chrono::system_clock::to_time_t(timeNow);

  // Construct log entry
  LogEntry logEntry{ .time = now, .level = level };

  // Format original message
  {
#if defined(COMPILER_MSC)
    va_list arg_ptr;
#else
    __builtin_va_list arg_ptr;
#endif

    // Ensure message is clear
    mem_zero((char*)(logEntry.msg), MSG_LENGTH);

    va_start(arg_ptr, msg);
    vsnprintf((char*)(logEntry.msg), MSG_LENGTH, msg, arg_ptr);
    va_end(arg_ptr);
  }

  // Print log entry to console...
  PrintTime(logEntry.time);
  PrintLogEntry(logEntry);
}

static void LoggingThreadRun()
{
  while (true)
  {
    // Wait until there is a log entry in the queue or the thread should exit
    std::unique_lock lock(queueMutex);
    loggingCV.wait(lock, [](){ return loggingThreadShouldExit || logQueue.entryCount > 0; });

    // Copy log queue to local copy, reset queue and unlock mutex...
    mem_copy(&localLogQueue, &logQueue, sizeof(LogQueue));
    logQueue.entryCount = 0;
    lock.unlock();

    // Print all logs in local queue...
    for (u32 i = 0; i < localLogQueue.entryCount; ++i)
    {
      PrintTime(localLogQueue.queue[i].time);
      PrintLogEntry(localLogQueue.queue[i]);
    }

    if (loggingThreadShouldExit)
    {
      // NOTE(WSWhitehouse): Checking that no new logs have been entered before shutting down...
      queueMutex.lock();
      const u32 entryCount = logQueue.entryCount;
      queueMutex.unlock();

      if (entryCount > 0) continue;

      return;
    }
  }
}

static INLINE void PrintTime(const std::time_t& time)
{
  // Get current local time
  struct std::tm* localTime = std::localtime(&time);
  if (localTime == nullptr) return;

  // https://www.techiedelight.com/print-current-date-and-time-in-c/
  const i32& hour  = localTime->tm_hour;
  const i32& min   = localTime->tm_min;
  const i32& sec   = localTime->tm_sec;
  const i32& day   = localTime->tm_mday;
  const i32& month = localTime->tm_mon  + 1;
  const i32& year  = localTime->tm_year + 1900;

  // NOTE(WSWhitehouse): Because we're not changing any console attributes, there is
  // no need for platform specific console output here. Using the C std lib instead.
  FILE* stream = stdout; //level > LOG_LEVEL_ERROR ? stdout : stderr;
  fprintf(stream, "[%02d/%02d/%d %02d:%02d:%02d]", day, month, year, hour, min, sec);
}

// NOTE(WSWhitehouse): Different platforms handle printing to the console differently, each
// platform should have their own implementation of the `PrintLogEntry()` function below...

#if defined(PLATFORM_WINDOWS)

#include <windows.h>
static INLINE void PrintLogEntry(const LogEntry& logEntry)
{
  // NOTE(WSWhitehouse): Unfortunately, not all Windows terminals support ASCII escape and format
  // codes. Therefore, we rely on the old `SetConsoleTextAttribute` function - this requires a bit
  // more setup and the message must be printed using the Windows console handles!

  // Get console handle
  DWORD handleType     = STD_OUTPUT_HANDLE; //level > LOG_LEVEL_ERROR ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
  HANDLE consoleHandle = GetStdHandle(handleType);

  // Cache the original console info, so we can reset it afterwards
  CONSOLE_SCREEN_BUFFER_INFO originalConsoleInfo;
  GetConsoleScreenBufferInfo(consoleHandle, &originalConsoleInfo);

  // Set console attributes
  const u8 colours[] = { 64, 4, 6, 2, 1, 96 }; // FATAL, ERROR, WARN, INFO, DEBUG, PROFILE
  SetConsoleTextAttribute(consoleHandle, colours[logEntry.level]);

  // Append level string to front of va_msg
  mem_zero(outMsg, MSG_LENGTH);
  snprintf(outMsg, MSG_LENGTH, " %s%s \n", LevelStrings[logEntry.level], logEntry.msg);

  // Print to console
  OutputDebugStringA(outMsg);
  DWORD length          = (DWORD)str_length(outMsg);
  LPDWORD numberWritten = nullptr;
  WriteConsoleA(consoleHandle, outMsg, length, numberWritten, nullptr);
  FlushConsoleInputBuffer(consoleHandle);

  // Reset console attributes to the cached values...
  SetConsoleTextAttribute(consoleHandle, originalConsoleInfo.wAttributes);
}

#else

static INLINE void PrintLogEntry(const LogEntry& logEntry)
{
  FILE* stream = stdout; //level > LOG_LEVEL_ERROR ? stdout : stderr;
  const char* colourStr[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;33"}; // FATAL, ERROR, WARN, INFO, DEBUG, PROFILE
  fprintf(stream, "\033[%sm %s%s \033[0m\n", colourStr[logEntry.level], LevelStrings[logEntry.level], logEntry.msg);
}

#endif