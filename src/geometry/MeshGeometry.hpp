#ifndef SNOWFLAKE_MESH_GEOMETRY_HPP
#define SNOWFLAKE_MESH_GEOMETRY_HPP

#include "pch.hpp"
#include "core/Logging.hpp"
#include "containers/FArray.hpp"

// Forward Declarations
struct Vertex;
struct BoundingBox3D;

enum class IndexType : u32
{
  U16_TYPE = 0,
  U32_TYPE = 1
};


/** @brief A struct to hold the raw geometry data of a mesh.*/
struct MeshGeometry
{
  // Geometry Arrays
  Vertex* vertexArray = nullptr;

  union
  {
    void* indexArray = nullptr;
    u16* indexArrayU16;
    u32* indexArrayU32;
  };

  // Array counts
  u64 vertexCount = 0;
  u64 indexCount  = 0;

  IndexType indexType = IndexType::U16_TYPE;

  // --- Util Functions --- //

  /**
  * @brief The index array can be of varying types, this function
  * returns a "universal" index by converting the index into a u32.
  * This is designed for use-cases where you don't care about the
  * specific index type. It's best not to call this in a loop as it
  * performs the check every time it's called.
  * @param i Index to lookup into the index array.
  * @return u32 universal index.
  */
  [[nodiscard]] INLINE u32 GetUniversalIndex(u64 i) const
  {
    switch (indexType)
    {
      case IndexType::U16_TYPE: return (u32)((indexArrayU16)[i]);
      case IndexType::U32_TYPE: return (u32)((indexArrayU32)[i]);
    }

    LOG_FATAL("GetUniversalIndex: Unhandled Index Type Case! Returning 0.");
    return 0;
  }

  /**
  * @brief Get the `sizeof()` the index type. The index can be stored
  * as varying types, this function returns the appropriate Size
  * depending on that type.
  * @return Size of index type.
  */
  [[nodiscard]] INLINE u64 SizeOfIndex() const
  {
    switch (indexType)
    {
      case IndexType::U16_TYPE: return sizeof(u16);
      case IndexType::U32_TYPE: return sizeof(u32);
    }

    LOG_FATAL("SizeOfIndex: Unhandled Index Type Case! Returning 0.");
    return 0;
  }

  /** @brief Calculate the 3D bounding box of the mesh geometry. */
  [[nodiscard]] BoundingBox3D CalculateBoundingBox(const glm::mat4& transform = glm::identity<glm::mat4>()) const;

  /**
  * @brief Holds all the extreme vertex indices.
  * @see MeshGeometry::FindExtremeVertices
  */
  union ExtremeVertexIndices
  {
    FArray<u64, 6> indices = {};

    struct
    {
      u64 posX, posY, posZ;
      u64 negX, negY, negZ;
    };
  };

  /**
  * @brief Find the extreme vertices on all axis.
  * @return Returns a ExtremeVertexIndices object with the indices to all extreme vertices.
  * @see MeshGeometry::ExtremeVertexIndices
  */
  [[nodiscard]] ExtremeVertexIndices FindExtremeVertices(const glm::mat4& transform = glm::identity<glm::mat4>()) const;
};

#endif //SNOWFLAKE_MESH_GEOMETRY_HPP
