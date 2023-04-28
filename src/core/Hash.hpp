#ifndef SNOWFLAKE_HASH_HPP
#define SNOWFLAKE_HASH_HPP

#include "pch.hpp"

// NOTE(WSWhitehouse):
// FNV1a Hashing:
//   - https://gist.github.com/ruby0x1/81308642d0325fd386237cfa3b44785c
//   - https://notes.underscorediscovery.com/constexpr-fnv1a/

namespace Hash
{

  // --- CONST VALUES --- //
  constexpr const u32 FNV1a32_HASH_VALUE  = 0x811c9dc5;
  constexpr const u32 FNV1a32_PRIME_VALUE = 0x1000193;
  constexpr const u64 FNV1a64_HASH_VALUE  = 0xcbf29ce484222325;
  constexpr const u64 FNV1a64_PRIME_VALUE = 0x100000001b3;

  INLINE constexpr u32 FNV1a32(const void* data, const u32 length) noexcept
  {
    const byte* dataPtr = (byte*)data;

    u32 hash  = FNV1a32_HASH_VALUE;
    u32 prime = FNV1a32_PRIME_VALUE;

    for(u32 i = 0; i < length; ++i)
    {
      u8 value = dataPtr[i];

      hash  = hash ^ value;
      hash *= prime;
    }

    return hash;
  }

  INLINE constexpr u64 FNV1a64(const void* data, const u64 length) noexcept
  {
    const byte* dataPtr = (byte*)data;

    u64 hash  = FNV1a64_HASH_VALUE;
    u64 prime = FNV1a64_PRIME_VALUE;

    for(u64 i = 0; i < length; ++i)
    {
      u8 value = dataPtr[i];

      hash  = hash ^ value;
      hash *= prime;
    }

    return hash;
  }

  INLINE constexpr u32 FNV1a32Str(const char* const str, const u32 value = FNV1a32_HASH_VALUE) noexcept
  {
    return (str[0] == '\0') ? value : FNV1a32Str(&str[1], (value ^ u32(str[0])) * FNV1a32_PRIME_VALUE);
  }

  INLINE constexpr u64 FNV1a64Str(const char* const str, const u64 value = FNV1a64_HASH_VALUE) noexcept
  {
    return (str[0] == '\0') ? value : FNV1a64Str(&str[1], (value ^ u64(str[0])) * FNV1a64_PRIME_VALUE);
  }


} // namespace Hash

#endif //SNOWFLAKE_HASH_HPP
