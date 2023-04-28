#ifndef SNOWFLAKE_COMPONENT_CREATE_INFO_HPP
#define SNOWFLAKE_COMPONENT_CREATE_INFO_HPP

#include "pch.hpp"
#include "containers/FArray.hpp"
#include "renderer/Texture2D.hpp"
#include "ecs/components/Sprite.hpp"

struct SpriteCreateInfo
{
  const char** texturePath;
  u32 textureCount;

  SamplerFilter samplerFilter;
  Sprite::BillboardType billboardType;
};

struct UIImageCreateInfo
{
  const char** texturePath;
  u32 textureCount;
  SamplerFilter samplerFilter;
};

struct SkyboxCreateInfo
{
  union
  {
    FArray<const char*, 6> texturePaths = {};

    struct
    {
      const char* texturePathPosX;
      const char* texturePathNegX;
      const char* texturePathPosY;
      const char* texturePathNegY;
      const char* texturePathPosZ;
      const char* texturePathNegZ;
    };
  };
};

#endif //SNOWFLAKE_COMPONENT_CREATE_INFO_HPP
