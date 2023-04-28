/**
 * @file language_detection.h
 *
 * @brief Detect the language and version this code is being compiled with.
 *
 * Language macros are defined following the pattern (where 'x' is the language name):
 *   #define LANGUAGE_x
 * 
 * Language version macros are defined separate following the pattern (where 'x' is
 * the language name, and 'y' is the version):
 *    #define LANGUAGE_xy
 * 
 * A separate macro named 'LANGUAGE_VER' is defined to the version number of the
 * language that is being compiled (i.e. if the program is compiled in version 14 
 * of C++, 'LANGUAGE_VER' is defined as 14).
 *
 * Supported Languages:
 * - CPP:
 *   - LANGUAGE_CPP
 * - C:
 *   - LANGUAGE_C
 */

#ifndef LANGUAGE_DETECTION_H
#define LANGUAGE_DETECTION_H

#if defined(__cplusplus)
  #define LANGUAGE_CPP 1

  #if __cplusplus == 201103L // NOTE(WSWhitehouse): not supported by MSVC
    #define LANGUAGE_CPP11 1
    #define LANGUAGE_VER 11

  #elif __cplusplus == 201402L
    #define LANGUAGE_CPP14 1
    #define LANGUAGE_VER 14

  #elif __cplusplus == 201703L
    #define LANGUAGE_CPP17 1
    #define LANGUAGE_VER 17

  #elif __cplusplus <= 202002L
    #define LANGUAGE_CPP20 1
    #define LANGUAGE_VER 20

  #else
    #define LANGUAGE_CPP_UNKNOWN 1
    #define LANGUAGE_VER 0
  #endif

#else
  #define LANGUAGE_C 1

  #if !__STDC_VERSION__
    #define LANGUAGE_C89 1
    #define LANGUAGE_VER 89

  #elif __STDC_VERSION__ == 199901L
    #define LANGUAGE_C99 1
    #define LANGUAGE_VER 99

  #elif __STDC_VERSION__ == 201112L
    #define LANGUAGE_C11 1
    #define LANGUAGE_VER 11

  #elif __STDC_VERSION__ == 201710L
    #define LANGUAGE_C17 1
    #define LANGUAGE_VER 17

  #elif __STDC_VERSION__ > 201710L // TODO: update this when the actual version number is finalized
    #define LANGUAGE_C23 1
    #define LANGUAGE_VER 23

  #else
    #define LANGUAGE_C_UNKNOWN 1
    #define LANGUAGE_VER 0
  #endif
#endif

#endif // LANGUAGE_DETECTION_H
