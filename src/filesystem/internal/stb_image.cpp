// NOTE(WSWhitehouse): This is the implementation file for the stb_image.h library. It sets up the
// header with the correct memory and assert functions and defines the implementation macro. There
// is no header file to associate with this source file, just include the stb_image.h header file
// as normal! Do NOT include the implementation in any other file!

#include "common.h"
#include "core/Assert.hpp"

// Define memory functions
#define STBI_MALLOC mem_alloc
#define STBI_REALLOC mem_realloc
#define STBI_FREE mem_free

// Define assert
#define STBI_ASSERT(x) ASSERT_MSG((x), "STB Image assert failed!")

// Include stb with its implementation
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"