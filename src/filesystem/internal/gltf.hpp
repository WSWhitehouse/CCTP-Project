#ifndef SNOWFLAKE_GLTF_HPP
#define SNOWFLAKE_GLTF_HPP

#include "pch.hpp"
#include "core/Hash.hpp"
#include "threading/JobSystem.hpp"

// std
#include <mutex>

// json
#include "rapidjson/document.h"

/**
* NOTE(WSWhitehouse):
* This file contains all the structs and constants for loading a gltf file, most
* of the information and layouts are gathered from the gltf spec. Check the spec
* for more information and updates to the format:
*   https://registry.khronos.org/glTF/
*/

// NOTE(WSWhitehouse): Using a "flexible array member" in gltf::Chunk::chunkData[] to
// allow the array to "overflow" the struct if the chunk data is located alongside the
// chunk in the gltf file. This feature is a C99 extension, which is why it must be
// explicitly enabled here...
#if defined(COMPILER_CLANG)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wc99-extensions"
#endif

namespace gltf
{
  #define GLTF_MIN_SUPPORTED_VER 2

  // --- GLTF FILE TYPES --- //
  static constexpr const u32 GLB_EXTENSION  = Hash::FNV1a32Str("glb");
  static constexpr const u32 GLTF_EXTENSION = Hash::FNV1a32Str("gltf");
  static constexpr const u32 BIN_EXTENSION  = Hash::FNV1a32Str("bin");

  enum class FileType : u32
  {
    UNKNOWN,
    BINARY, // glb file extension
    JSON    // gltf file extension
  };

  // --- FILE CHUNKS --- //
  struct Header
  {
    static constexpr const u32 MAGIC_VALUE = 0x46546C67;

    u32 magic;
    u32 version;
    u32 length;
  };

  struct Chunk
  {
    enum class Type : u32
    {
      JSON = 0x4E4F534A,
      BIN  = 0x004E4942
    };

    u32 chunkLength;
    Type chunkType;
    byte chunkData[];
  };

  // --- JSON OBJECTS --- //

  // NOTE(WSWhitehouse): The titles of the json objects, they are here to avoid spelling
  // mistakes. However, some names are *very* similar (i.e. "bufferView" and "bufferViews")
  static constexpr const char* NODES_STR          = "nodes";
  static constexpr const char* ACCESSORS_STR      = "accessors";
  static constexpr const char* MESH_STR           = "mesh";
  static constexpr const char* MATRIX_STR         = "matrix";
  static constexpr const char* TRANSLATION_STR    = "translation";
  static constexpr const char* ROTATION_STR       = "rotation";
  static constexpr const char* SCALE_STR          = "scale";
  static constexpr const char* MESHES_STR         = "meshes";
  static constexpr const char* BUFFER_VIEWS_STR   = "bufferViews";
  static constexpr const char* BUFFERS_STR        = "buffers";
  static constexpr const char* PRIMITIVES_STR     = "primitives";
  static constexpr const char* INDICES_STR        = "indices";
  static constexpr const char* ATTRIBUTES_STR     = "attributes";
  static constexpr const char* BUFFER_VIEW_STR    = "bufferView";
  static constexpr const char* COMPONENT_TYPE_STR = "componentType";
  static constexpr const char* COUNT_STR          = "count";
  static constexpr const char* TYPE_STR           = "type";
  static constexpr const char* BUFFER_STR         = "buffer";
  static constexpr const char* BYTE_LENGTH_STR    = "byteLength";
  static constexpr const char* BYTE_OFFSET_STR    = "byteOffset";
  static constexpr const char* URI_STR            = "uri";

  // NOTE(WSWhitehouse): Sections inside json are referred to as Objects, this is followed
  // in the naming of the following structures. They represent the "code version" of the
  // equivalent json objects...

  struct JsonNode
  {
    u32 meshIndex;
    glm::mat4 transformMatrix;
  };

  struct JsonMesh
  {
    enum class Attribute : u32
    {
      POSITION = 0,
      NORMAL,
      TANGENT,
      TEXCOORD,
      COLOR,

      COUNT
    };

    static constexpr const char* AttributeStr[(u32)Attribute::COUNT]
    {
      "POSITION",
      "NORMAL",
      "TANGENT",
      "TEXCOORD_0",
      "COLOR_0"
    };

    /** @brief If an accessor index equals this value, it doesn't exist and should be ignored. */
    static constexpr const i32 INVALID_ACCESSOR_INDEX = -1;

    i32 attributeAccessorIndices[(u32)Attribute::COUNT];
    i32 indicesAccessorIndex;
  };

  struct JsonAccessor
  {
    enum class ComponentType : u32
    {
      BYTE           = 5120,
      UNSIGNED_BYTE  = 5121,
      SHORT          = 5122,
      UNSIGNED_SHORT = 5123,
      UNSIGNED_INT   = 5125,
      FLOAT          = 5126
    };

    enum class Type : u32
    {
      SCALAR  = Hash::FNV1a32Str("SCALAR"),
      VEC2    = Hash::FNV1a32Str("VEC2"),
      VEC3    = Hash::FNV1a32Str("VEC3"),
      VEC4    = Hash::FNV1a32Str("VEC4"),
      MAT2    = Hash::FNV1a32Str("MAT2"),
      MAT3    = Hash::FNV1a32Str("MAT3"),
      MAT4    = Hash::FNV1a32Str("MAT4"),
    };

    u32 bufferView;
    ComponentType componentType;
    u32 count;
    Type type;
    u32 stride;

    [[nodiscard]] static constexpr INLINE u32 GetTypeElementCount(Type type)
    {
      switch (type)
      {
        case Type::SCALAR: return 1;
        case Type::VEC2:   return 2;
        case Type::VEC3:   return 3;
        case Type::VEC4:   return 4;
        case Type::MAT2:   return 4;
        case Type::MAT3:   return 9;
        case Type::MAT4:   return 16;

        default:
        {
          LOG_FATAL("Unhandled case in gltf::Accessor::GetTypeElementCount!");
          return 0;
        }
      }
    }

    /** @brief Returns the stride for the passed in ComponentType param. */
    [[nodiscard]] static constexpr INLINE u32 GetComponentTypeStride(ComponentType compType)
    {
      switch (compType)
      {
        case ComponentType::BYTE:           return sizeof(sbyte);
        case ComponentType::UNSIGNED_BYTE:  return sizeof(byte);
        case ComponentType::SHORT:          return sizeof(i16);
        case ComponentType::UNSIGNED_SHORT: return sizeof(u16);
        case ComponentType::UNSIGNED_INT:   return sizeof(u32);
        case ComponentType::FLOAT:          return sizeof(f32);

        default:
        {
          LOG_FATAL("Unhandled case in gltf::Accessor::GetComponentTypeStride!");
          return 0;
        }
      }
    }
  };

  struct JsonBufferView
  {
    u32 buffer;
    u32 byteLength;
    u32 byteOffset;
  };

  struct JsonBuffer
  {
    enum class DataType : u32
    {
      BINARY,
      SEPARATE,
      EMBEDDED
    };

    u32 byteLength;
    DataType dataType;
    byte* data;
  };

  // --- GLTF STRUCT --- //
  struct JsonGltf
  {
    // NOTE(WSWhitehouse): The following pointers are arrays of each json
    // object, with their count in the corresponding count variable.

    JsonMesh* meshes;
    JsonAccessor* accessors;
    JsonBufferView* bufferViews;
    JsonBuffer* buffers;

    u32 meshesCount;
    u32 accessorsCount;
    u32 bufferViewsCount;
    u32 buffersCount;

    /**
    * @brief Allocates the arrays for the JsonGltf structure. This is more
    * efficient than allocating memory manually, use the associated Free()
    * function to release the memory once it is no longer needed!
    * @param _meshesCount Element count for meshes array
    * @param _accessorsCount Element count for accessors array
    * @param _bufferViewsCount Element count for buffer views array
    * @param _buffersCount Element count for buffers array
    */
    INLINE void Allocate(const u32& _meshesCount, const u32& _accessorsCount,
                         const u32& _bufferViewsCount, const u32& _buffersCount)
    {
      meshesCount      = _meshesCount;
      accessorsCount   = _accessorsCount;
      bufferViewsCount = _bufferViewsCount;
      buffersCount     = _buffersCount;

      // NOTE(WSWhitehouse): Doing one big allocation here rather than a load of
      // smaller ones. This is for a few reasons:
      //   - Quicker memory allocation.
      //   - Only one memory free later.
      //   - All of this data is frequently accessed together, so the data is more
      //     likely to be on the same cache line.

      const u32 meshesSize      = sizeof(JsonMesh)       * meshesCount;
      const u32 accessorsSize   = sizeof(JsonAccessor)   * accessorsCount;
      const u32 bufferViewsSize = sizeof(JsonBufferView) * bufferViewsCount;
      const u32 buffersSize     = sizeof(JsonBuffer)     * buffersCount;

      const u32 totalSize = meshesSize + accessorsSize + bufferViewsSize + buffersSize;
      const byte* memory  = (byte*)mem_alloc(totalSize);

      // NOTE(WSWhitehouse): The pointer arithmetic is done in this way to allow the
      // CPU to perform out-of-order operations when assigning the pointers...
      meshes      = (JsonMesh*)       (memory);
      accessors   = (JsonAccessor*)   (memory + (meshesSize));
      bufferViews = (JsonBufferView*) (memory + (meshesSize + accessorsSize));
      buffers     = (JsonBuffer*)     (memory + (meshesSize + accessorsSize + bufferViewsSize));
    }

    /** @brief Free the memory associated with this struct. */
    INLINE void Free()
    {
      if (meshes == nullptr)
      {
        LOG_ERROR("Trying to free gltf structure that hasn't been allocated!");
        return;
      }

      // NOTE(WSWhitehouse): As all the arrays are allocated in one big chunk of
      // memory, only the meshes pointer needs to be freed. See the Allocate()
      // function above for more details.
      mem_free(meshes);

      meshes      = nullptr;
      accessors   = nullptr;
      bufferViews = nullptr;
      buffers     = nullptr;

      meshesCount      = 0;
      accessorsCount   = 0;
      bufferViewsCount = 0;
      buffersCount     = 0;
    }
  };

  // --- UTIL FUNCTIONS --- //

  // Forward Declarations
  INLINE FileType GetFileType(const char* const filePath);
  INLINE Chunk* GetFirstChunk(const FileSystem::FileContent& fileContent);
  INLINE Chunk* AdvanceToNextChunk(const Chunk* const chunk);
  INLINE b8 IsHeaderValid(const Header* const header);
  INLINE b8 ProcessJson(rapidjson::Document& json, JsonGltf* out_gltf);
  INLINE b8 DoRequiredJsonRootObjectsExist(const rapidjson::Document& json);
  INLINE b8 MeshFromJson(const rapidjson::Document::ValueType& json, JsonMesh* out_mesh);
  INLINE b8 AccessorFromJson(const rapidjson::Document::ValueType& json, JsonAccessor* out_accessor);
  INLINE b8 BufferViewFromJson(const rapidjson::Document::ValueType& json, JsonBufferView* out_bufferView);
  INLINE b8 BufferFromJson(const rapidjson::Document::ValueType& json, JsonBuffer* out_buffer);

  INLINE FileType GetFileType(const char* const filePath)
  {
    const char* const extension = FileSystem::GetFileExtension(filePath);
    if (extension == nullptr) return FileType::UNKNOWN;

    const u32 extHash = Hash::FNV1a32(extension, str_length(extension));

    if (extHash == GLB_EXTENSION)  return FileType::BINARY;
    if (extHash == GLTF_EXTENSION) return FileType::JSON;

    return FileType::UNKNOWN;
  }

  /**
  * @brief Get a pointer to the first chunk in the file content.
  * @param fileContent File Content to use for accessing first chunk.
  * @return Pointer to start of first chunk.
  */
  INLINE Chunk* GetFirstChunk(const FileSystem::FileContent& fileContent)
  {
    return (Chunk*)(fileContent.data + sizeof(Header));
  }

  /**
  * @brief Provides the ptr to the next chunk, does not perform
  * any overflow checks. Therefore, ensure there is another chunk
  * available before advancing!
  * @param chunk Chunk to advance from.
  * @return Pointer to start of next chunk.
  */
  INLINE Chunk* AdvanceToNextChunk(const Chunk* const chunk)
  {
    return (Chunk*)(chunk->chunkData + chunk->chunkLength);
  }

  /**
  * @brief Checks the values in the header block to ensure they are valid.
  * @param header Header to check.
  * @return True when header is valid; false otherwise.
  */
  INLINE b8 IsHeaderValid(const Header* const header)
  {
    if (header->magic != Header::MAGIC_VALUE)
    {
      LOG_FATAL("Gltf magic value not recognised! Please check file format.");
      return false;
    }

    if (header->version != GLTF_MIN_SUPPORTED_VER)
    {
      LOG_FATAL("Gltf version not supported! Minimum supported version: %u", GLTF_MIN_SUPPORTED_VER);
      return false;
    }

    return true;
  }

  /**
  * @brief Processes the json document and outputs a gltf struct containing all
  * of its information. This struct must have its internal array memory freed
  * when it is no longer needed.
  * @param json The json document to process.
  * @param out_gltf The pointer to the out gltf. Must not be nullptr.
  * @return True on success; false otherwise.
  */
  INLINE b8 ProcessJson(rapidjson::Document& json, JsonGltf* out_gltf)
  {
    using namespace rapidjson;

    if (!DoRequiredJsonRootObjectsExist(json))
    {
      LOG_ERROR("Failed to process gltf json. Required json root objects don't exist...");
      return false;
    }

    const Document::Array jsonMeshes      = json[gltf::MESHES_STR].GetArray();
    const Document::Array jsonAccessors   = json[gltf::ACCESSORS_STR].GetArray();
    const Document::Array jsonBufferViews = json[gltf::BUFFER_VIEWS_STR].GetArray();
    const Document::Array jsonBuffers     = json[gltf::BUFFERS_STR].GetArray();

    const u32 meshesCount      = jsonMeshes.Size();
    const u32 accessorsCount   = jsonAccessors.Size();
    const u32 bufferViewsCount = jsonBufferViews.Size();
    const u32 buffersCount     = jsonBuffers.Size();

    if (meshesCount <= 0 || accessorsCount <= 0 || bufferViewsCount <= 0 || buffersCount <= 0)
    {
      LOG_ERROR("Failed to process gltf json. Some (or all) required json root objects have 0 elements...");
      return false;
    }

    JsonGltf gltf = {};
    gltf.Allocate(meshesCount, accessorsCount, bufferViewsCount, buffersCount);

    // Process the json arrays and input the data into the gltf struct...

    // NOTE(WSWhitehouse): The json processing below could be done asynchronously. However, after
    // a few tests - its slower to start tasks and wait for the threads to complete them. Meaning
    // the json processing is so fast that creating jobs is actually slower - for most meshes.

    // Meshes
    for (u32 i = 0; i < gltf.meshesCount; i++)
    {
      [[likely]] if (MeshFromJson(jsonMeshes[i], &gltf.meshes[i])) continue;

      gltf.Free();
      return false;
    }

    // Accessors
    for (u32 i = 0; i < gltf.accessorsCount; i++)
    {
      [[likely]] if (AccessorFromJson(jsonAccessors[i], &gltf.accessors[i])) continue;

      gltf.Free();
      return false;
    }

    // Buffer Views
    for (u32 i = 0; i < gltf.bufferViewsCount; i++)
    {
      [[likely]] if (BufferViewFromJson(jsonBufferViews[i], &gltf.bufferViews[i])) continue;

      gltf.Free();
      return false;
    }

    // Buffers
    for (u32 i = 0; i < gltf.buffersCount; i++)
    {
      [[likely]] if (BufferFromJson(jsonBuffers[i], &gltf.buffers[i])) continue;

      gltf.Free();
      return false;
    }

    (*out_gltf) = gltf;
    return true;
  }

  /**
  * @brief Ensures the root objects in the json that are required
  * for loading a mesh exist. The spec states they are not required
  * but are needed for mesh loading in snowflake.
  * @param json The json document to check.
  * @return true if all required root objects exist; false otherwise.
  */
  INLINE b8 DoRequiredJsonRootObjectsExist(const rapidjson::Document& json)
  {
    if (!json.HasMember(ACCESSORS_STR))    return false;
    if (!json.HasMember(MESHES_STR))       return false;
    if (!json.HasMember(BUFFER_VIEWS_STR)) return false;
    if (!json.HasMember(BUFFERS_STR))      return false;

    return true;
  }

  INLINE b8 MeshFromJson(const rapidjson::Document::ValueType& json, JsonMesh* out_mesh)
  {
    using namespace rapidjson;

    if (!json.HasMember(PRIMITIVES_STR))
    {
      LOG_ERROR("Failed to process gltf json. Mesh object doesn't contain a primitives section, the spec states it must!");
      return false;
    }

    auto primitives = json[PRIMITIVES_STR].GetArray();

    JsonMesh mesh = {};

    // TODO(WSWhitehouse): Support multiple primitives.
    {
      const Document::Array::ValueType& primitive = primitives[0];

      if (!primitive.HasMember(ATTRIBUTES_STR))
      {
        LOG_ERROR("Failed to process gltf json. Mesh primitive object doesn't contain any attributes, the spec states it must!");
        return false;
      }

      // Get attribute accessor indices...
      const Document::ValueType& attributes = primitive[ATTRIBUTES_STR];
      for (u32 attributeIndex = 0; attributeIndex < (u32)JsonMesh::Attribute::COUNT; ++attributeIndex)
      {
        // NOTE(WSWhitehouse): It is not required that the mesh contains all the attributes, if this is
        // the case set the index to the invalid value so it can be safely ignored later.
        if (!attributes.HasMember(JsonMesh::AttributeStr[attributeIndex]))
        {
          mesh.attributeAccessorIndices[attributeIndex] = JsonMesh::INVALID_ACCESSOR_INDEX;
          continue;
        }

        const rapidjson::Document::ValueType& value   = attributes[JsonMesh::AttributeStr[attributeIndex]];
        mesh.attributeAccessorIndices[attributeIndex] = value.GetInt();
      }

      // NOTE(WSWhitehouse): Even though we don't need all attributes, the position one is important, as
      // this is the vertex positions. Without this attribute we can't load a mesh.
      if (mesh.attributeAccessorIndices[(u32)JsonMesh::Attribute::POSITION] == JsonMesh::INVALID_ACCESSOR_INDEX)
      {
        LOG_ERROR("Failed to process gltf json. Mesh attributes doesn't contain a position! This is required.");
        return false;
      }

      // Get indices index
      [[likely]]
      if (primitive.HasMember(INDICES_STR))
      {
        mesh.indicesAccessorIndex = primitive[INDICES_STR].GetInt();
      }
      else
      {
        mesh.indicesAccessorIndex = JsonMesh::INVALID_ACCESSOR_INDEX;
      }
    }

    (*out_mesh) = mesh;
    return true;
  }

  INLINE b8 AccessorFromJson(const rapidjson::Document::ValueType& json, JsonAccessor* out_accessor)
  {
    // NOTE(WSWhitehouse): Check all required json sections exist...
    if (!json.HasMember(BUFFER_VIEW_STR))    return false;
    if (!json.HasMember(COMPONENT_TYPE_STR)) return false;
    if (!json.HasMember(COUNT_STR))          return false;
    if (!json.HasMember(TYPE_STR))           return false;

    JsonAccessor accessor  = {};
    accessor.bufferView    = json[BUFFER_VIEW_STR].GetInt();
    accessor.componentType = (JsonAccessor::ComponentType)json[COMPONENT_TYPE_STR].GetInt();
    accessor.count         = json[COUNT_STR].GetInt();

    // Find the accessor type
    const char* typeString = json[TYPE_STR].GetString();
    accessor.type = (JsonAccessor::Type)Hash::FNV1a32(typeString, str_length(typeString));

//    for (u32 i = 0; i < (u32)JsonAccessor::Type::COUNT; ++i)
//    {
//      if (JsonAccessor::TypeStr[i] == nullptr) continue;
//
//      // TODO(WSWhitehouse): Probably want to do some string hashing here instead of
//      // str_equal, it is faster and the hashes can be precomputed on the code side...
//      if (str_equal(typeString, JsonAccessor::TypeStr[i]))
//      {
//        accessor.type = (JsonAccessor::Type)i;
//        break;
//      }
//    }

    // NOTE(WSWhitehouse): Calculating the stride here so it only needs to be computed once.
    accessor.stride = JsonAccessor::GetComponentTypeStride(accessor.componentType) *
                      JsonAccessor::GetTypeElementCount(accessor.type);

    (*out_accessor) = accessor;
    return true;
  }
  
  INLINE b8 BufferViewFromJson(const rapidjson::Document::ValueType& json, JsonBufferView* out_bufferView)
  {
    // NOTE(WSWhitehouse): Check all required json sections exist...
    if (!json.HasMember(BUFFER_STR))      return false;
    if (!json.HasMember(BYTE_LENGTH_STR)) return false;

    JsonBufferView bufferView = {};
    bufferView.buffer     = json[BUFFER_STR].GetInt();
    bufferView.byteLength = json[BYTE_LENGTH_STR].GetInt();

    // NOTE(WSWhitehouse): Having the byte offset field is not required as there
    // may only be one buffer view, in which case the offset is not specified.
    bufferView.byteOffset = 0;
    if (json.HasMember(BYTE_OFFSET_STR))
    {
      bufferView.byteOffset = json[BYTE_OFFSET_STR].GetInt();
    }

    (*out_bufferView) = bufferView;
    return true;
  }

  INLINE b8 BufferFromJson(const rapidjson::Document::ValueType& json, JsonBuffer* out_buffer)
  {
    // NOTE(WSWhitehouse): Check all required json sections exist...
    if (!json.HasMember(BYTE_LENGTH_STR)) return false;

    JsonBuffer buffer = {};
    buffer.byteLength = json[BYTE_LENGTH_STR].GetInt();

    [[likely]]
    if (!json.HasMember(URI_STR))
    {
      buffer.dataType = JsonBuffer::DataType::BINARY;
    }
    else
    {
      // TODO(WSWhitehouse): Add support for this option.
    }

    (*out_buffer) = buffer;
    return true;
  }

} // namespace gltf

#if defined(COMPILER_CLANG)
  #pragma clang diagnostic pop
#endif

#endif //SNOWFLAKE_GLTF_HPP
