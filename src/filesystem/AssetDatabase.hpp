#ifndef SNOWFLAKE_ASSET_DATABASE_HPP
#define SNOWFLAKE_ASSET_DATABASE_HPP

#include "pch.hpp"
#include "filesystem/RawAssetData.hpp"

// Forward Declarations
struct Mesh;

namespace AssetDatabase
{

  b8 Init();
  void Shutdown();

  Mesh* LoadMesh(const char* filePath);

  TextureData* LoadTexture(const char* filePath);
  void FreeTexture(TextureData* textureData);

  // TODO(WSWhitehouse): Implement loading from audio files generically rather than in the AudioManager...
  AudioData* LoadAudio(const char* filePath);
  void FreeAudio(AudioData* audioData);

} // namespace AssetDatabase


#endif //SNOWFLAKE_ASSET_DATABASE_HPP
