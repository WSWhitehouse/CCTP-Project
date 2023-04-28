#include "filesystem/AssetDatabase.hpp"

// core
#include "core/Logging.hpp"
#include "core/Abort.hpp"
#include "core/Assert.hpp"

// filesystem
#include "filesystem/FileSystem.hpp"
#include "filesystem/internal/gltf.hpp"

// geometry
#include "geometry/Vertex.hpp"
#include "geometry/Mesh.hpp"

// other
#include "stb_image.h"
#include "rapidjson/document.h"
#include <gtc/type_ptr.hpp> // glm::make_mat4, glm::make_vec3, glm::make_quat

// Forward Declarations
static b8 LoadJsonNodes(Mesh* mesh, rapidjson::Document& json);
static void LoadJsonMesh(Mesh* mesh, const gltf::JsonGltf& gltf);

constexpr const char* DataDirectory = "data/";

b8 AssetDatabase::Init()
{
  if (!FileSystem::DirectoryExists(DataDirectory))
  {
    // NOTE(WSWhitehouse): Logging immediately here to ensure this message is seen before the program closes...
    LOG_IMMEDIATE(Logging::LOG_LEVEL_FATAL, "Data Directory does NOT exist! Have you run the `data` target in CMake?");
    return false;
  }

  return true;
}

void AssetDatabase::Shutdown()
{
  // TODO(WSWhitehouse): Do clean up of all loaded assets...
}

Mesh* AssetDatabase::LoadMesh(const char* filePath)
{
  if (!FileSystem::FileExists(filePath))
  {
    LOG_ERROR("AssetDatabase: Mesh file does not exist! File path: %s", filePath);
    return nullptr;
  }

  FileSystem::FileContent fileContent;
  if (!FileSystem::ReadAllFileContent(filePath, &fileContent))
  {
    LOG_ERROR("AssetDatabase: Unable to read file at path: %s", filePath);
    return nullptr;
  }

  const gltf::FileType fileType = gltf::GetFileType(filePath);

  rapidjson::Document json;
  gltf::JsonGltf gltf = {};

  switch (fileType)
  {
    [[likely]]
    case gltf::FileType::BINARY:
    {
      gltf::Header* header = (gltf::Header*)fileContent.data;
      if (!gltf::IsHeaderValid(header))
      {
        mem_free(fileContent.data);
        return nullptr;
      }

      // NOTE(WSWhitehouse): Getting the first chunk which MUST be the json chunk...
      gltf::Chunk* jsonChunk = gltf::GetFirstChunk(fileContent);
      if (jsonChunk->chunkType != gltf::Chunk::Type::JSON)
      {
        LOG_ERROR("First gltf chunk is not of Json type! The spec states the first chunk MUST be of Json type!");
        mem_free(fileContent.data);
        return nullptr;
      }

      // Parse json chunk data
      json.Parse((const char*)jsonChunk->chunkData, jsonChunk->chunkLength);
      break;
    }

    case gltf::FileType::JSON:
    {
      LOG_WARN("Using json gltf files is slow. Please use binary instead...");
      json.Parse((const char*)fileContent.data, fileContent.size);
      break;
    }

    case gltf::FileType::UNKNOWN:
    default:
    {
      LOG_ERROR("Gltf file loading failed. Unable to determine gltf file type! Please check the file extension");
      mem_free(fileContent.data);
      return nullptr;
    }
  }

  if (!gltf::ProcessJson(json, &gltf))
  {
    mem_free(fileContent.data);
    return nullptr;
  }

  // Get Buffer Pointers
  for (u32 i = 0; i < gltf.buffersCount; ++i)
  {
    gltf::JsonBuffer& buffer = gltf.buffers[i];

    // NOTE(WSWhitehouse): We need to do different things depending on the buffers data type. The buffer
    // data could be in an external .bin file, or inline in the json (hopefully this is not the case as
    // it's the slowest option). Or the buffer is in another chunk in the original file (preferred option).

    switch (buffer.dataType)
    {
      [[likely]]
      case gltf::JsonBuffer::DataType::BINARY:
      {
        gltf::Chunk* jsonChunk = gltf::GetFirstChunk(fileContent);
        gltf::Chunk* binChunk  = gltf::AdvanceToNextChunk(jsonChunk);
        if (binChunk->chunkType != gltf::Chunk::Type::BIN)
        {
          LOG_ERROR("The second data chunk in the gltf file is not of Binary type! The spec states this MUST be the case.");
          gltf.Free();
          mem_free(fileContent.data);
          return nullptr;
        }

        if (buffer.byteLength != binChunk->chunkLength)
        {
          LOG_WARN("Buffer byte length != Bin chunk length. You might want to check that out...");
        }

        buffer.data = binChunk->chunkData;
        break;
      }

      case gltf::JsonBuffer::DataType::SEPARATE:
      {
        // TODO(WSWhitehouse): Add support for this option.
        LOG_FATAL("The gltf file format is not currently supported!");
        gltf.Free();
        mem_free(fileContent.data);
        return nullptr;

        break;
      }

      [[unlikely]]
      case gltf::JsonBuffer::DataType::EMBEDDED:
      {
        LOG_WARN("Gltf binary data is embedded in the json data. This is **VERY** slow, please use binary or separate instead!");

        // TODO(WSWhitehouse): Add support for this option.
        LOG_FATAL("The gltf file format is not currently supported!");
        gltf.Free();
        mem_free(fileContent.data);
        return nullptr;

        break;
      }
    }
  }

  Mesh* mesh = (Mesh*)mem_alloc(sizeof(Mesh));

  // NOTE(WSWhitehouse): Asynchronously loading the Json Nodes. No need to lock the
  // boolean here as it should only be accessed after the job is complete.
  bool jsonNodesFailed = false;
  JobSystem::WorkFuncPtr jsonNodesLambda = [&]
  {
    if (!LoadJsonNodes(mesh, json))
    {
      jsonNodesFailed = true;
    }
  };

  JobSystem::JobHandle jsonNodesJob = JobSystem::SubmitJob(jsonNodesLambda);

  LoadJsonMesh(mesh, gltf);
  jsonNodesJob.WaitUntilComplete();

  // No longer need these resources
  gltf.Free();
  mem_free(fileContent.data);

  // NOTE(WSWhitehouse): Check the status of the json nodes, if they have failed
  // free the mesh pointer and return nullptr. Technically, in a failure state
  // the entire mesh object has been processed already. But failures are the
  // exception not the rule...
  if (jsonNodesFailed)
  {
    mem_free(mesh);
    return nullptr;
  }

  return mesh;
}

TextureData* AssetDatabase::LoadTexture(const char* filePath)
{
  if (!FileSystem::FileExists(filePath))
  {
    LOG_ERROR("AssetDatabase: Texture file does not exist! File path: %s", filePath);
    return nullptr;
  }

  FileSystem::FileContent fileContent;
  if (!FileSystem::ReadAllFileContent(filePath, &fileContent))
  {
    LOG_ERROR("AssetDatabase: Unable to read file at path: %s", filePath);
    return nullptr;
  }

  TextureData* data = (TextureData*)mem_alloc(sizeof(TextureData));

  i32 width, height, channels;
  data->pixels = stbi_load_from_memory(fileContent.data, (i32)fileContent.size,
                                       &width, &height, &channels, STBI_rgb_alpha);

  mem_free(fileContent.data);

  // Failed to load pixel data
  if (data->pixels == nullptr)
  {
    mem_free(data);
    return nullptr;
  }

  data->width    = (u32)width;
  data->height   = (u32)height;
  data->channels = (u32)channels;

  return data;
}

void AssetDatabase::FreeTexture(TextureData* textureData)
{
  stbi_image_free(textureData->pixels);
  mem_free(textureData);
}

static b8 LoadJsonNodes(Mesh* mesh, rapidjson::Document& json)
{
  using namespace rapidjson;

  if (!json.HasMember(gltf::NODES_STR)) return false;

  const Document::Array jsonNodes = json[gltf::NODES_STR].GetArray();
  const u32 jsonNodesCount        = jsonNodes.Size();

  // NOTE(WSWhitehouse): Nodes may not contain any mesh related data, in that
  // case we don't care about them and they shouldn't be included in the Mesh
  // object. Therefore, we must calculate the actual "mesh node" count.

  u32 meshNodes = 0;
  for (u32 i = 0; i < jsonNodesCount; i++)
  {
    const Document::ValueType& jsonNode = jsonNodes[i];
    if (!jsonNode.HasMember(gltf::MESH_STR)) continue;

    meshNodes++;
  }

  if (meshNodes <= 0)
  {
    LOG_ERROR("Gltf failed to load. There are no mesh nodes in the file.");
    return false;
  }

  mesh->nodeCount = meshNodes;
  mesh->nodeArray = (MeshNode*)(mem_alloc(sizeof(MeshNode) * mesh->nodeCount));

  // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-node

  u32 currentNode = 0;
  for (u32 i = 0; i < jsonNodesCount; i++)
  {
    const Document::ValueType& jsonNode = jsonNodes[i];
    if (!jsonNode.HasMember(gltf::MESH_STR)) continue;

    // NOTE(WSWhitehouse): As the json node array is not guaranteed to align with
    // the mesh node array, use separate index variable that is only updated when
    // its a valid mesh node.
    MeshNode& node = mesh->nodeArray[currentNode];
    currentNode++;

    node.geometryIndex   = jsonNode[gltf::MESH_STR].GetInt();
    node.transformMatrix = glm::mat4(1.0f);

    // NOTE(WSWhitehouse): The transformation matrix can be stored in either a matrix
    // variable in the node or as individual transform, rotation and scale properties.
    // All of these are optional, but if there is a matrix there aren't any individual
    // properties (and vice versa). Another note: not all individual properties have
    // to exist in the node either.

    if (jsonNode.HasMember(gltf::MATRIX_STR))
    {
      const auto matrixArray = jsonNode[gltf::MATRIX_STR].GetArray();

      f32 matrix[16];
      for (int matrixIter = 0; matrixIter < 16; ++matrixIter)
      {
        matrix[i] = matrixArray[i].GetFloat();
      }

      // NOTE(WSWhitehouse): The array in json is stored in column-major order, so
      // there is no need to transpose it, etc...
      node.transformMatrix = glm::make_mat4(matrix);

      continue; // No need to check for individual transform properties.
    }

    glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation    = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 scale       = glm::vec3(1.0f, 1.0f, 1.0f);

    // translation
    if (jsonNode.HasMember(gltf::TRANSLATION_STR))
    {
      const auto& valueArray = jsonNode[gltf::TRANSLATION_STR].GetArray();

      f32 values[3];
      for (u32 iter = 0; iter < 3; iter++)
      {
        // NOTE(WSWhitehouse): Gltf stores single digit floating point numbers, but rapidjson
        // interprets them as integers. Using GetFloat() on what it sees as an integer causes
        // an assert...
        if (valueArray[iter].IsInt())
        {
          values[iter] = (f32)valueArray[iter].GetInt();
        }
        else
        {
          values[iter] = valueArray[iter].GetFloat();
        }
      }

      translation = glm::make_vec3(values);
    }

    // rotation
    if (jsonNode.HasMember(gltf::ROTATION_STR))
    {
      const auto& valueArray = jsonNode[gltf::ROTATION_STR].GetArray();

      f32 values[4];
      for (u32 iter = 0; iter < 4; iter++)
      {
        // NOTE(WSWhitehouse): Gltf stores single digit floating point numbers, but rapidjson
        // interprets them as integers. Using GetFloat() on what it sees as an integer causes
        // an assert...
        if (valueArray[iter].IsInt())
        {
          values[iter] = (f32)valueArray[iter].GetInt();
        }
        else
        {
          values[iter] = valueArray[iter].GetFloat();
        }
      }

      rotation = glm::make_quat(values);
    }

    // scale
    if (jsonNode.HasMember(gltf::SCALE_STR))
    {
      const auto& valueArray = jsonNode[gltf::SCALE_STR].GetArray();

      f32 values[3];
      for (u32 iter = 0; iter < 3; iter++)
      {
        // NOTE(WSWhitehouse): Gltf stores single digit floating point numbers, but rapidjson
        // interprets them as integers. Using GetFloat() on what it sees as an integer causes
        // an assert...
        if (valueArray[iter].IsInt())
        {
          values[iter] = (f32)valueArray[iter].GetInt();
        }
        else
        {
          values[iter] = valueArray[iter].GetFloat();
        }
      }

      scale = glm::make_vec3(values);
    }

    const glm::mat4 identity          = glm::mat4(1.0f);
    const glm::mat4 scaleMatrix       = glm::scale(identity, scale);
    const glm::mat4 rotateMatrix      = glm::mat4_cast(rotation);
    const glm::mat4 translationMatrix = glm::translate(identity, translation);

    node.transformMatrix = translationMatrix * rotateMatrix * scaleMatrix;
  }

  return true;
}

static void LoadJsonMesh(Mesh* mesh, const gltf::JsonGltf& gltf)
{
  // Initialise Mesh
  mesh->geometryCount = gltf.meshesCount;
  mesh->geometryArray = (MeshGeometry*)mem_alloc(sizeof(MeshGeometry) * mesh->geometryCount);

  for (u32 meshIter = 0; meshIter < mesh->geometryCount; meshIter++)
  {
    const gltf::JsonMesh& jsonMesh = gltf.meshes[meshIter];
    MeshGeometry& meshGeometry     = mesh->geometryArray[meshIter];

    // Allocate vertex memory...
    {
      const i32& accessorIndex = jsonMesh.attributeAccessorIndices[(u32) gltf::JsonMesh::Attribute::POSITION];
      const gltf::JsonAccessor& positionAccessor = gltf.accessors[accessorIndex];

      const u64 arraySize      = sizeof(Vertex) * positionAccessor.count;
      meshGeometry.vertexCount = positionAccessor.count;
      meshGeometry.vertexArray = (Vertex*) mem_alloc(arraySize);

      // NOTE(WSWhitehouse): Setting all the vertices to a default value...
      for (u64 i = 0; i < meshGeometry.vertexCount; i++)
      {
        meshGeometry.vertexArray[i] =
          Vertex(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec2(0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
          );
      }
    }

    for (u32 attributeIndex = 0; attributeIndex < (u32)gltf::JsonMesh::Attribute::COUNT; attributeIndex++)
    {
      const gltf::JsonMesh::Attribute attributeType = (gltf::JsonMesh::Attribute)attributeIndex;
      const i32& accessorIndex = jsonMesh.attributeAccessorIndices[attributeIndex];

      // NOTE(WSWhitehouse): Ensure the attribute is valid
      if (accessorIndex == gltf::JsonMesh::INVALID_ACCESSOR_INDEX) continue;

      const gltf::JsonAccessor& accessor     = gltf.accessors[accessorIndex];
      const gltf::JsonBufferView& bufferView = gltf.bufferViews[accessor.bufferView];
      const gltf::JsonBuffer& buffer         = gltf.buffers[bufferView.buffer];

      const u32& stride = accessor.stride;
      const byte* data  = &buffer.data[bufferView.byteOffset];

      for (u32 dataCount = 0; dataCount < accessor.count; dataCount++)
      {
        void* attributePtr = nullptr;

        switch (attributeType)
        {
          case gltf::JsonMesh::Attribute::POSITION:
          {
            attributePtr = &meshGeometry.vertexArray[dataCount].position;
            break;
          }
          case gltf::JsonMesh::Attribute::NORMAL:
          {
            attributePtr = &meshGeometry.vertexArray[dataCount].normal;
            break;
          }
          case gltf::JsonMesh::Attribute::TEXCOORD:
          {
            attributePtr = &meshGeometry.vertexArray[dataCount].texcoord;
            break;
          }
          case gltf::JsonMesh::Attribute::COLOR:
          {
            attributePtr = &meshGeometry.vertexArray[dataCount].colour;
            break;
          }

            // Unsupported types
          case gltf::JsonMesh::Attribute::TANGENT:
          default: break;
        }

        if (attributePtr != nullptr)
        {
          mem_copy(attributePtr, data, stride);
        }

        // NOTE(WSWhitehouse): Inverting UV y...
        /*
        if (attributeType == gltf::JsonMesh::Attribute::TEXCOORD)
        {
          glm::vec2* uv = (glm::vec2*)attributePtr;
          uv->y = 1.0f - uv->y;
        }
        */

        data += stride;
      }
    }

    // Indices List
    if (jsonMesh.indicesAccessorIndex != gltf::JsonMesh::INVALID_ACCESSOR_INDEX)
    {
      const u32& indicesAccessorIndex    = jsonMesh.indicesAccessorIndex;
      const gltf::JsonAccessor& accessor = gltf.accessors[indicesAccessorIndex];

      const gltf::JsonBufferView& bufferView = gltf.bufferViews[accessor.bufferView];
      const gltf::JsonBuffer& buffer         = gltf.buffers[bufferView.buffer];

      const byte* const data = &buffer.data[bufferView.byteOffset];

      // NOTE(WSWhitehouse): The indices can be in varying component types, this switch statement
      // ensures we load the indices at the correct size and set up the mesh geometry correctly.

      u64 sizeOfIndex = 0;
      switch (accessor.componentType)
      {
        case gltf::JsonAccessor::ComponentType::UNSIGNED_SHORT:
        {
          meshGeometry.indexType = IndexType::U16_TYPE;
          sizeOfIndex            = sizeof(u16);
          break;
        }
        case gltf::JsonAccessor::ComponentType::UNSIGNED_INT:
        {
          meshGeometry.indexType = IndexType::U32_TYPE;
          sizeOfIndex            = sizeof(u32);
          break;
        }

        default:
        {
          LOG_FATAL("Loading GLTF mesh index array with unhandled component type! (%s:%i)", __FILE__, __LINE__);
          break;
        }
      }

      meshGeometry.indexCount = accessor.count;
      meshGeometry.indexArray = mem_alloc(sizeOfIndex * accessor.count);

      mem_copy(meshGeometry.indexArray, data, bufferView.byteLength);
    }
  }
}

static constexpr const u32 WAV_EXT_HASH = Hash::FNV1a32Str("wav");
AudioData* AssetDatabase::LoadAudio(const char* filePath)
{
  if (!FileSystem::FileExists(filePath))
  {
    LOG_ERROR("AssetDatabase: Audio file does not exist! File path: %s", filePath);
    return nullptr;
  }

  const char* const extension = FileSystem::GetFileExtension(filePath);
  const u32 extensionHash     = Hash::FNV1a32Str(extension);
  if (extensionHash != WAV_EXT_HASH)
  {
    LOG_ERROR("AssetDatabase: Unsupported Audio file type! Can only load '.wav'! File path: %s", filePath);
    return nullptr;
  }

  FileSystem::FileContent fileContent;
  if (!FileSystem::ReadAllFileContent(filePath, &fileContent))
  {
    LOG_ERROR("AssetDatabase: Unable to read file at path: %s", filePath);
    return nullptr;
  }

  AudioData* audioData = (AudioData*)mem_alloc(sizeof(AudioData));

  byte* audioFile = fileContent.data;

  const u32* riff = (u32*)audioFile;
  if (endian_htob32(*riff) != 'RIFF')
  {
    LOG_ERROR("AssetDatabase: Audio File is not of type 'RIFF'! Failed to load at path: %s", filePath);
    mem_free(fileContent.data);
    mem_free(audioData);
    return nullptr;
  }

  // Advance file pointer...
  audioFile += sizeof(u32);

  const u32* fileSize = (u32*)audioFile;
  if (fileContent.size < *fileSize)
  {
    LOG_ERROR("AssetDatabase: Audio File does not contain a valid file size! Perhaps it's corrupt! Failed to load at path: %s", filePath);
    mem_free(fileContent.data);
    mem_free(audioData);
    return nullptr;
  }

  // Advance file pointer...
  audioFile += sizeof(u32);

  const u32* wave = (u32*)audioFile;
  if (endian_htob32(*wave) != 'WAVE')
  {
    LOG_ERROR("AssetDatabase: Audio File is not of type 'WAVE'! Failed to load at path: %s", filePath);
    mem_free(fileContent.data);
    mem_free(audioData);
    return nullptr;
  }

  // Advance file pointer...
  audioFile += sizeof(u32);

  const u32* fmt = (u32*)audioFile;
  if (endian_htob32(*fmt) != 'fmt ')
  {
    LOG_ERROR("AssetDatabase: Audio File does not contain a 'FMT' section! Failed to load at path: %s", filePath);
    mem_free(fileContent.data);
    mem_free(audioData);
    return nullptr;
  }

  // Advance file pointer...
  audioFile += sizeof(u32);

  const u32* fmtSize   = (u32*)audioFile;
  const u32 fmtLength  = *fmtSize / sizeof(byte);
  audioData->fmt       = (byte*)mem_alloc(fmtLength);
  audioData->fmtLength = fmtLength;

  // Advance file pointer...
  audioFile += sizeof(u32);

  mem_copy(audioData->fmt, audioFile, fmtLength);

  // Advance file pointer...
  audioFile += fmtLength;

  const u32* data = (u32*)audioFile;
  if (endian_htob32(*data) != 'data')
  {
    LOG_ERROR("AssetDatabase: Audio File does not contain a 'DATA' section! Failed to load at path: %s", filePath);
    mem_free(fileContent.data);
    mem_free(audioData->fmt);
    mem_free(audioData);
    return nullptr;
  }

  // Advance file pointer...
  audioFile += sizeof(u32);

  const u32* dataSize   = (u32*)audioFile;
  const u32 dataLength  = *dataSize / sizeof(byte);
  audioData->data       = (byte*)mem_alloc(dataLength);
  audioData->dataLength = dataLength;

  // Advance file pointer...
  audioFile += sizeof(u32);

  mem_copy(audioData->data, audioFile, dataLength);

  mem_free(fileContent.data);
  return audioData;
}

void AssetDatabase::FreeAudio(AudioData* audioData)
{
  mem_free(audioData->fmt);
  mem_free(audioData->data);
  mem_free(audioData);
}