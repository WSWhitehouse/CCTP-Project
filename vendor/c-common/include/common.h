#ifndef COMMON_H
#define COMMON_H

#if defined(__cplusplus) 
extern "C" {
#endif

// Standard Lib Includes
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

// Detection
#include "common/detection/compiler_detection.h"
#include "common/detection/language_detection.h"
#include "common/detection/platform_detection.h"

// Common
#include "common/types.h"
#include "common/defines.h"
#include "common/bitflag.h"
#include "common/mem.h"

// System
#include "common/sys_endian.h"

// Libs
#include "common/strlib.h"
#include "common/arraylib.h"

// Maths
#include "common/maths/maths_defines.h"

#if defined(__cplusplus) 
}
#endif

#endif // COMMON_H
