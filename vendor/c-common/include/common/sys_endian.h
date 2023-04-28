/**
 * @file sys_endian.h
 * 
 * @brief Provides platform-independent functions for converting integers to big/little 
 * endian to/from host. Provides functions for determining if a platform has big or little
 * endian. Supports all platforms detected in platform_detection.h.
 * 
 * Determine endianness functions:
 *   - endian_is_big()
 *   - endian_is_little()
 * 
 * Conversion functions:
 *   endian_[b/l/h]to[b/l/h][bits]() : where b = big, l = little, h = host, bits = int size (16, 32, 64)
 * 
 *   16-bit Functions:
 *     - endian_htob16() : Host to Big endian
 *     - endian_htol16() : Host to Little endian
 *     - endian_btoh16() : Big endian to Host
 *     - endian_ltoh16() : Little endian to Host
 *   32-bit Functions:
 *     - endian_htob32() : Host to Big endian
 *     - endian_htol32() : Host to Little endian
 *     - endian_btoh32() : Big endian to Host
 *     - endian_ltoh32() : Little endian to Host
 *   64-bit Functions:
 *     - endian_htob64() : Host to Big endian
 *     - endian_htol64() : Host to Little endian
 *     - endian_btoh64() : Big endian to Host
 *     - endian_ltoh64() : Little endian to Host
 */

#ifndef SYS_ENDIAN_H
#define SYS_ENDIAN_H

#include "common/detection/platform_detection.h"
#include "common/types.h"
#include "common/defines.h"

/** @brief Returns true if platform is big endian. */
static INLINE b8 endian_is_big() 
{
  const int i = 1;
  return (*(char*)&i) == 0;
}

/** @brief Returns true if platform is little endian. */
static INLINE b8 endian_is_little() 
{
  return !endian_is_big();
}

#if defined(PLATFORM_WINDOWS)
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32")

  #if BYTE_ORDER == LITTLE_ENDIAN
    #define endian_htob16(val) htons(val)
    #define endian_htol16(val)      (val)
    #define endian_btoh16(val) ntohs(val)
    #define endian_ltoh16(val)      (val)

    #define endian_htob32(val) htonl(val)
    #define endian_htol32(val)      (val)
    #define endian_btoh32(val) ntohl(val)
    #define endian_ltoh32(val)      (val)

    #define endian_htob64(val) htonll(val)
    #define endian_htol64(val)       (val)
    #define endian_btoh64(val) ntohll(val)
    #define endian_ltoh64(val)       (val)
  #elif BYTE_ORDER == BIG_ENDIAN
    #define endian_htob16(val)                  (val)
    #define endian_htol16(val) __builtin_bswap16(val)
    #define endian_btoh16(val)                  (val)
    #define endian_ltoh16(val) __builtin_bswap16(val)

    #define endian_htob32(val)                  (val)
    #define endian_htol32(val) __builtin_bswap32(val)
    #define endian_btoh32(val)                  (val)
    #define endian_ltoh32(val) __builtin_bswap32(val)

    #define endian_htob64(val)                  (val)
    #define endian_htol64(val) __builtin_bswap64(val)
    #define endian_btoh64(val)                  (val)
    #define endian_ltoh64(val) __builtin_bswap64(val)
  #else
    #error Byte order not supported on Windows Platform!
  #endif
#endif

#if defined(PLATFORM_APPLE)
  #include <libkern/OSByteOrder.h>

  #define endian_htob16(val) OSSwapHostToBigInt16(val)
  #define endian_htol16(val) OSSwapHostToLittleInt16(val)
  #define endian_btoh16(val) OSSwapBigToHostInt16(val)
  #define endian_ltoh16(val) OSSwapLittleToHostInt16(val)

  #define endian_htob32(val) OSSwapHostToBigInt32(val)
  #define endian_htol32(val) OSSwapHostToLittleInt32(val)
  #define endian_btoh32(val) OSSwapBigToHostInt32(val)
  #define endian_ltoh32(val) OSSwapLittleToHostInt32(val)

  #define endian_htob64(val) OSSwapHostToBigInt64(val)
  #define endian_htol64(val) OSSwapHostToLittleInt64(val)
  #define endian_btoh64(val) OSSwapBigToHostInt64(val)
  #define endian_ltoh64(val) OSSwapLittleToHostInt64(val)
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_BSD)
  // https://man7.org/linux/man-pages/man3/endian.3.html

  #if defined(PLATFORM_LINUX)
   #include <endian.h>
  #endif

  #if defined(PLATFORM_BSD)
    #include <sys/endian.h>

    // NOTE(WSWhitehouse): NetBSD & FreeBSD include the same header as other BSD 
    // platforms but use a different format for their function names. We define 
    // macros with the "standard" name so they can all be changed below at once.
    #if defined(PLATFORM_NETBSD) || defined(PLATFORM_FREEBSD)
      #define be16toh(val) betoh16(val)
      #define le16toh(val) letoh16(val)
      #define be32toh(val) betoh32(val)
      #define le32toh(val) letoh32(val)
      #define be64toh(val) betoh64(val)
      #define le64toh(val) letoh64(val)
    #endif
  #endif

  #define endian_htob16(val) htobe16(val)
  #define endian_htol16(val) htole16(val)
  #define endian_btoh16(val) be16toh(val)
  #define endian_ltoh16(val) le16toh(val)

  #define endian_htob32(val) htobe32(val)
  #define endian_htol32(val) htole32(val)
  #define endian_btoh32(val) be32toh(val)
  #define endian_ltoh32(val) le32toh(val)

  #define endian_htob64(val) htobe64(val)
  #define endian_htol64(val) htole64(val)
  #define endian_btoh64(val) be64toh(val)
  #define endian_ltoh64(val) le64toh(val)

#endif

#endif // SYS_ENDIAN_H
