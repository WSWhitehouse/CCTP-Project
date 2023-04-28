#include "common.h"
#include <string.h>
#include <ctype.h> // isspace

u64 str_length(const char* str)
{
  return strlen(str);
}

char* str_duplicate(const char* str) 
{
  u64 length = str_length(str);
  char* copy = mem_alloc(length + 1);
  mem_copy(copy, str, length);
  copy[length] = '\0';
  return copy;
}

i32 str_compare(const char* str0, const char* str1) 
{
  return strcmp(str0, str1);
}

b8 str_equal(const char* str0, const char* str1) 
{
  return str_compare(str0, str1) == 0;
}

b8 str_equal_ignore_case(const char* str0, const char* str1) 
{
  i32 i;
  for (i = 0; str0[i] != '\0'; i++) 
  {
    if (tolower(str0[i]) != tolower(str1[i])) return false;
  }

  return str1[i] == '\0';
}

void str_copy(char* dest, const char* src)
{
  strcpy(dest, src);
}

char* str_trim(char* str) 
{
  while (isspace((uchar)*str)) 
  {
    str++;
  }

  if (*str) 
  {
    char* p = str;

    while (*p) 
    {
      p++;
    }

    while (isspace((uchar)*(--p)))
      ;

    p[1] = '\0';
  }

  return str;
}

i32 str_index_of(char* str, char c) 
{
  u64 length = str_length(str);
  if (length > 0) 
  {
    for (u64 i = 0; i < length; ++i) 
    {
      if (str[i] == c) { return i; }
    }
  }

  return -1;
}

i32 str_index_of_last(char* str, char c) 
{
  u64 length = str_length(str);
  if (length > 0) 
  {
    for (u64 i = length - 1; i > 0; --i) 
    {
      if (str[i] == c) { return i; }
    }
  }

  return -1;
}

void str_to_upper(char* str) 
{
  for (i32 i = 0; str[i] != '\0'; ++i) 
  {
    str[i] = toupper(str[i]);
  }
}

void str_to_lower(char* str) 
{
  for (i32 i = 0; str[i] != '\0'; i++) 
  {
    str[i] = tolower(str[i]);
  }
}
