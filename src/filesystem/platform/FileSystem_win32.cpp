#include "filesystem/FileSystem.hpp"

#if defined(PLATFORM_WINDOWS)

#include "core/Logging.hpp"

#include <windows.h>
#include <shlwapi.h> // PathFileExistsA

b8 FileSystem::FileExists(const char* filePath)
{
  return (b8)PathFileExistsA(filePath);
}

b8 FileSystem::DirectoryExists(const char* dirPath)
{
  return (b8)PathFileExistsA(dirPath);
}

#endif