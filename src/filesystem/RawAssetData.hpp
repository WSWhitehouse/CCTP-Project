#ifndef SNOWFLAKE_RAW_ASSET_DATA_HPP
#define SNOWFLAKE_RAW_ASSET_DATA_HPP

#include "pch.hpp"

struct TextureData
{
  byte* pixels;

  u32 width;
  u32 height;
  u32 channels;
};

struct AudioData
{
  byte* fmt;
  byte* data;

  u32 fmtLength;
  u32 dataLength;
};

#endif //SNOWFLAKE_RAW_ASSET_DATA_HPP
