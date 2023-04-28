#ifndef SNOWFLAKE_FILE_SYSTEM_HPP
#define SNOWFLAKE_FILE_SYSTEM_HPP

#include "pch.hpp"

namespace FileSystem
{
  /** @brief File contents */
  struct FileContent
  {
    byte* data;
    u32 size;
  };

  /**
  * @brief Reads an entire file and outputs it to FileContent.
  * @param out_fileContent Output content of file.
  * @param filePath Path of file to open and read contents.
  * @return Returns true if file was successfully read, false if not.
  */
  b8 ReadAllFileContent(const char* filePath, FileContent* out_fileContent);

  /**
  * @brief Gets the extension of the provided file path, returns a pointer
  * to the char in the filepath were the extension begins. Therefore, there
  * is no need to individually free the pointer that is returned and no new
  * memory is allocated!
  * @param filePath File path to search for the extension.
  * @return Returns a pointer to the first character of the extension, returns
  * nullptr on failure - file path is not valid or an extension doesn't exist.
  */
  const char* GetFileExtension(const char* filePath);

  /**
  * @brief Checks if the file at the specified path exists.
  * @param filePath Path to check for a file.
  * @return True when file exists; false otherwise.
  */
  b8 FileExists(const char* filePath);

  /**
  * @brief Checks if the directory at the specified path exists.
  * @param dirPath Path to check for a directory.
  * @return True when directory exists; false otherwise.
  */
  b8 DirectoryExists(const char* dirPath);

} // namespace FileSystem

#endif //SNOWFLAKE_FILE_SYSTEM_HPP
