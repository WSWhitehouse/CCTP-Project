#ifndef SNOWFLAKE_BSP_TREE_HPP
#define SNOWFLAKE_BSP_TREE_HPP

#include "pch.hpp"

// containers
#include "containers/FArray.hpp"
#include "containers/DArray.hpp"

// geometry
#include "geometry/MeshGeometry.hpp"
#include "geometry/Plane.hpp"

// Forward Declarations
struct Mesh;
struct MeshGeometry;
struct Vertex;

#define MAX_TRIANGLES 50U

class BSPTree
{
public:
  void BuildTree(const Mesh* mesh);

  struct Node
  {
    b8 isLeaf   = false;
    Plane plane = {};

    // NOTE(WSWhitehouse): Should only link to other nodes if this is not a leaf
    u32 nodePositive = U32_MAX;
    u32 nodeNegative = U32_MAX;

    // NOTE(WSWhitehouse): Should only contain geometry if this is a leaf node
    u32 indexCount = 0;
    u32* indices = nullptr;//FArray<u32, (u64)(MAX_INDICES)> indices = {};
  };

  DArray<Node> nodes = {};

  Vertex* vertices = nullptr;
  u64 vertexCount  = 0;
};

#endif //SNOWFLAKE_BSP_TREE_HPP
