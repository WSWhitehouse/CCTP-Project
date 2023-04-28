#ifndef SNOWFLAKE_FLAG_HPP
#define SNOWFLAKE_FLAG_HPP

#include "pch.hpp"

#include "core/Logging.hpp"

#include "threading/Thread.hpp"

#include <memory>
#include <atomic>

namespace Threading
{

  /**
  * @brief A shared state flag to check the status of asynchronous operations.
  * Once the flag has been initialised, it's future can be obtained (see
  * 'Flag::GetFuture()'). The future can retrieve the status of the async
  * operation. Warning: improper use or discarding the Flag after a future
  * has been obtained can lead to deadlocks - ensure the flag is set before
  * discarding.
  */
  struct Flag
  {

    /** @brief The future that checks the status of the flag. */
    struct Future
    {
      Future()  = default;
      ~Future() = default;

      // Delete class copy
      Future(const Future& other)            = delete;
      Future& operator=(const Future& other) = delete;

      // Allow class move
      Future(Future&& other) noexcept            = default;
      Future& operator=(Future&& other) noexcept = default;

      // NOTE(WSWhitehouse): Friend struct of 'Flag' so it can set the atomic
      // flag variable when initialising the future (see 'Flag::GetFuture()').
      friend struct Flag;

      /** @brief Get the status of the future without blocking */
      [[nodiscard]] INLINE b8 Get()
      {
        if (!IsValid()) return true;

        const b8 flag = _atomicFlag->load(std::memory_order::seq_cst);
        if (flag) FreeFlag();

        return flag;
      }

      /** @brief Blocks thread and waits for the flag to be set. */
      INLINE void Wait()
      {
        while (IsValid())
        {
          const b8 flag = Get();
          if (flag) { return; }

//          LOG_INFO("waiting on flag...");
          _atomicFlag->wait(false);
        }
      }

      /** @brief Check if this future is valid. */
      [[nodiscard]] INLINE b8 IsValid() const { return _atomicFlag != nullptr; }

    private:
      std::atomic<b8>* _atomicFlag = nullptr;

      INLINE void FreeFlag()
      {
        mem_free(_atomicFlag);
        _atomicFlag = nullptr;
      }
    };

    /**
    * @brief Initialise the state of the flag. Must be called before
    * obtaining the flags future or setting its state.
    */
    INLINE void Init()
    {
      if (_atomicFlag != nullptr)
      {
        LOG_ERROR("Trying to reuse a Threading::Flag without setting it first!");
      }

      _atomicFlag = (std::atomic<b8>*)mem_alloc(sizeof(std::atomic<b8>));
      _atomicFlag->store(false, std::memory_order::seq_cst);
    }

    /**
    * @brief Get the future associated with this flag.
    * @return This flags future.
    */
    [[nodiscard]] INLINE Future GetFuture() const
    {
      Future future      = {};
      future._atomicFlag = _atomicFlag;
      return future;
    }

    /**
    * @brief Sets the flag and notifies any waiting threads.
    */
    INLINE void Set()
    {
      if (_atomicFlag == nullptr)
      {
        LOG_ERROR("Trying to set an invalid Threading::Flag!");
        return;
      }

      _atomicFlag->store(true, std::memory_order::seq_cst);
      _atomicFlag->notify_all();
      _atomicFlag = nullptr;
    }

  private:
    std::atomic<b8>* _atomicFlag = nullptr;
  };

} // namespace Threading

#endif //SNOWFLAKE_FLAG_HPP
