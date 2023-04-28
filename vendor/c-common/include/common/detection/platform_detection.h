/**
 * @file platform_detection.h
 * 
 * @brief Detect the platform this code is compiled on. Platform detection 
 * inspired by the following sources: 
 *   - https://github.com/qt/qtbase/blob/dev/src/corelib/global/qsystemdetection.h
 *   - https://stackoverflow.com/a/42040445/13195883
 * 
 * Platform macros are defined follwing the pattern (where 'x' is the platform name):
 *   #define PLATFORM_x 
 * 
 * Supported Platforms:
 * - Windows:
 *   - PLATFORM_WINDOWS
 *   - PLATFORM_WINDOWS_32
 *   - PLATFORM_WINDOWS_64
 * - Apple:
 *   - PLATFORM_APPLE
 *   - PLATFORM_APPLE_32
 *   - PLATFORM_APPLE_64
 *   - PLATFORM_IOS
 *   - PLATFORM_IOS_SIMULATOR
 *   - PLATFORM_MAC
 * - Solaris:
 *   - PLATFORM_SOLARIS
 * - Linux:
 *   - PLATFORM_LINUX
 *   - PLATFORM_ANDROID
 *   - PLATFORM_WEBOS
 * - UNIX / POSIX:
 *   - PLATFORM_UNIX
 *   - PLATFORM_POSIX
 * - BSD:
 *   - PLATFORM_BSD
 *   - PLATFORM_FREEBSD
 *   - PLATFORM_NETBSD
 *   - PLATFORM_OPENBSD
 *   - PLATFORM_INTERIX
 * - IBM AIX:
 *   - PLATFORM_AIX
 * - Cygwin:
 *   - PLATFORM_CYGWIN
 */

#ifndef PLATFORM_DETECTION_H
#define PLATFORM_DETECTION_H

// Windows
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
  #define PLATFORM_WINDOWS 1

  #if defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
    #define PLATFORM_WINDOWS_64 1
  #else 
    #define PLATFORM_WINDOWS_32 1
  #endif

// Apple
#elif __APPLE__
  #include <TargetConditionals.h>
  
  #define PLATFORM_APPLE 1

  #if defined(__LP64__)
    #define PLATFORM_APPLE_64 1
  #else
    #define PLATFORM_APPLE_32 1
  #endif

  // IOS Simulator
  #if TARGET_IPHONE_SIMULATOR
    #define PLATFORM_IOS 1
    #define PLATFORM_IOS_SIMULATOR 1

  // IOS
  #elif TARGET_OS_IPHONE
    #define PLATFORM_IOS 1

  // Mac
  #elif TARGET_OS_MAC
    #define PLATFORM_MAC 1

  #else
    #error "Unknown Apple platform"
  #endif

// Solaris
#elif defined(__sun) || defined(sun)
  #define PLATFORM_SOLARIS 1

// Linux
#elif defined(__linux__) || defined(__gnu_linux__) 
  #define PLATFORM_LINUX 1

  // Android
  #if defined(__ANDROID__) || defined(ANDROID)
    #define PLATFORM_ANDROID 1
  #endif

  // LG WebOS
  #if defined(__WEBOS__)
    #define PLATFORM_WEBOS 1
  #endif

// BSD Platforms
#elif defined(BSD) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || \
      defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__INTERIX)

  #define PLATFORM_BSD 1

  // Free BSD
  #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    #define PLATFORM_FREEBSD 1

  // Net BSD
  #elif defined(__NetBSD__)
    #define PLATFORM_NETBSD 1

  // Open BSD
  #elif defined(__OpenBSD__)
    #define PLATFORM_OPENBSD 1
    
  // Interix
  #elif defined(__INTERIX)
    #define PLATFORM_INTERIX 1
  #endif

// IBM AIX
#elif defined(_AIX)
  #define PLATFORM_AIX 1

// POSIX
#elif defined(_POSIX_VERSION)
  #define PLATFORM_POSIX 1

// Unknown
#else
  #error "Unknown platform!"
#endif

// Unix Platform
#if defined(unix) || defined(__unix__) || defined(__unix)
  #define PLATFORM_UNIX 1
#endif

// Cygwin
#if defined(__CYGWIN__)
  #define PLATFORM_CYGWIN 1
#endif

#endif // PLATFORM_DETECTION_H
