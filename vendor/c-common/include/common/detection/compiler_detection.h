/**
 * @file compiler_detection.h
 *
 * @brief Detect the compiler this code is being compiled with, code inspired by the following sources:
 *   - https://github.com/qt/qtbase/blob/dev/src/corelib/global/qcompilerdetection.h
 *
 * Compiler macros are defined follwing the pattern (where 'x' is the compiler name):
 *  #define COMPILER_x
 * 
 * The macros are defined to the compilers version using the COMPILER_MAKE_VER(MAJOR, MINOR, PATCH) macro,
 * this macro can be used to determine if the compiler is of a certain version. We use our own macro to 
 * make all version numbers consitent.
 *
 * Supported Compilers:
 * - Intel Compiler:
 *   - COMPILER_INTEL
 * - Clang:
 *   - COMPILER_CLANG
 * - GCC:
 *   - COMPILER_GCC
 * - Microsoft Compiler:
 *   - COMPILER_MSC
 * - GNU:
 *   - COMPILER_GNU
 * - MINGW (Doesn't support version):
 *   - COMPILER_MINGW
 */

#ifndef COMPILER_DETECTION_H
#define COMPILER_DETECTION_H

/**
 * @brief Creates a standardised version for all compilers. Used when defining the compiler type macro
 */
#define COMPILER_MAKE_VER(MAJOR, MINOR, PATCH) (((MAJOR) * 10000000) + ((MINOR) * 100000) + (PATCH))

// NOTE(WSWhitehouse): Defining all compilers if we're generating Doxygen
// so we get documentation comments on the compiler defines.
#if defined(DOXYGEN)
  /**
   * @brief Defined if using the Intel Compiler
   */
  #define COMPILER_INTEL

  /**
   * @brief Defined if using the Clang Compiler
   */
  #define COMPILER_CLANG

  /**
   * @brief Defined if using the GCC Compiler
   */
  #define COMPILER_GCC

  /**
   * @brief Defined if using the Microsoft Visual Compiler
   */
  #define COMPILER_MSC

  /**
   * @brief Defined if using the GNU Compiler
   */
  #define COMPILER_GNU

  /**
   * @brief Defined if using the MINGW Compiler
   */
  #define COMPILER_MINGW
#endif

// Intel
#if defined(__INTEL_COMPILER) // NOTE(WSWhitehouse): Intel pretends to be GNU/MSC, so checking that first
  #define COMPILER_INTEL COMPILER_MAKE_VER(__INTEL_COMPILER / 100, (__INTEL_COMPILER / 10) % 10, __INTEL_COMPILER % 10)
  
// Clang
#elif defined(__clang__) && defined(__clang_minor__)
  #define COMPILER_CLANG COMPILER_MAKE_VER(__clang_major__, __clang_minor__, __clang_patchlevel__)

// GCC
#elif defined(__gcc__)
  #define COMPILER_GCC 1 // TODO(WSWhitehouse): Figure out gcc version

// Microsoft Compiler
#elif defined(_MSC_VER) && defined(_MSC_FULL_VER)
  #if _MSC_VER == _MSC_FULL_VER / 10000
    #define COMPILER_MSC COMPILER_MAKE_VER(_MSC_VER / 100, _MSC_VER % 100, _MSC_FULL_VER % 10000)
  #else
    #define COMPILER_MSC COMPILER_MAKE_VER(_MSC_VER / 100, (_MSC_FULL_VER / 100000) % 100, _MSC_FULL_VER % 100000)
  #endif

// GNU
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
  #define COMPILER_GNU COMPILER_MAKE_VER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)

// MINGW
#elif defined(__MINGW64__)
  #define COMPILER_MINGW 64
#elif defined(__MINGW32__)
  #define COMPILER_MINGW 32

#else
  #error Unknown compiler!
#endif

#endif // COMPILER_DETECTION_H
