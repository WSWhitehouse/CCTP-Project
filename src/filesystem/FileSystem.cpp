#include "filesystem/FileSystem.hpp"
#include "core/Logging.hpp"

b8 FileSystem::ReadAllFileContent(const char* filePath, FileContent* out_fileContent)
{
  FILE* file = fopen(filePath, "rb");
  if (file == nullptr)
  {
    LOG_ERROR("File failed to open! (file path: %s)", filePath);
    return false;
  }

  // NOTE(WSWhitehouse): Clear the file content struct to ensure its empty and ready for use
  mem_zero(out_fileContent, sizeof(FileContent));

  // Get file size
  fseek(file, 0, SEEK_END);
  out_fileContent->size = (u32)ftell(file);
  rewind(file);

  // Read file data
  out_fileContent->data = (byte*)mem_alloc(sizeof(byte) * out_fileContent->size);
  fread(out_fileContent->data, sizeof(byte), out_fileContent->size, file);

  if (ferror(file))
  {
    LOG_ERROR("Error reading file! (file path: %s)", filePath);
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

const char* FileSystem::GetFileExtension(const char* filePath)
{
  if (filePath == nullptr) return nullptr;

  u64 pathLength = str_length(filePath);

  // NOTE(WSWhitehouse): We're counting any file path with less than 2 characters
  // as invalid as it's impossible to have a file name, the dot and extension in
  // this amount of chars.
  if (pathLength <= 2) return nullptr;

  for (u64 i = pathLength - 1; i > 0; i--)
  {
    const char& character = filePath[i];

    // Search for the dot (.) character...
    if(character != '.') continue;

    return &filePath[i + 1];
  }

  // Couldn't find file extension
  return nullptr;
}
