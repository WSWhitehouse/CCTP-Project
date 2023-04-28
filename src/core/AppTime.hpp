#ifndef SNOWFLAKE_APP_TIME_HPP
#define SNOWFLAKE_APP_TIME_HPP

#include "pch.hpp"
#include "Platform.hpp"
#include "core/Logging.hpp"

/**
 * @brief Struct which holds application times (frame and delta times, etc.)
 */
struct AppTime
{
  DECLARE_STATIC_CLASS(AppTime);

  /**
   * @brief Initialise the app time and start the timer.
   */
  static INLINE void Start()
  {
    LOG_INFO("App time initialised...");
    _appStartTime     = Platform::GetTime();
    _currentFrameTime = 0.0;
    _lastFrameTime    = 0.0;
    _deltaTime        = 0.0;
  }

  /**
   * @brief Update the app time. Should be called at
   * the beginning of the update loop.
   */
  static INLINE void Update()
  {
    _lastFrameTime    = _currentFrameTime;
    _currentFrameTime = Platform::GetTime() - _appStartTime;
    _appTotalTime     = _currentFrameTime - _appStartTime;
    _deltaTime        = _currentFrameTime - _lastFrameTime;
  }

  // Application Times
  [[nodiscard]] static const INLINE f64& AppStartTime() { return _appStartTime; }
  [[nodiscard]] static const INLINE f64& AppTotalTime() { return _appTotalTime; }

  // Frame Times
  [[nodiscard]] static const INLINE f64& CurrentFrameTime() { return _currentFrameTime; }
  [[nodiscard]] static const INLINE f64& LastFrameTime()    { return _lastFrameTime; }

  // Delta Times
  [[nodiscard]] static const INLINE f64& DeltaTime() { return _deltaTime; }

 private:
  // Application times
  static inline f64 _appStartTime = 0.0;
  static inline f64 _appTotalTime = 0.0;

  // Frame times
  static inline f64 _currentFrameTime = 0.0;
  static inline f64 _lastFrameTime    = 0.0;

  // Delta Times
  static inline f64 _deltaTime = 0.0;
};

#endif //SNOWFLAKE_APP_TIME_HPP
