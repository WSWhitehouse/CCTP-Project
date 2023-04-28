#include "containers/BSPTree.hpp"

// core
#include "core/Abort.hpp"
#include "core/Logging.hpp"
#include "core/Random.hpp"

// containers
#include "containers/FArray.hpp"
#include <queue>

// geometry
#include "geometry/BoundingBox3D.hpp"
#include "geometry/Mesh.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Triangle.hpp"
#include "geometry/Eigen.hpp"

// math
#include "math/Math.hpp"

// https://stackoverflow.com/a/21220521/13195883

// Forward Declarations
static INLINE Plane ChooseAutoPartitioningSplitPlane(const MeshGeometry& meshGeometry);
static INLINE Plane ChooseMaxVarianceSplitPlane(const MeshGeometry& meshGeometry);
static FArray<MeshGeometry, 2> SplitMesh(const MeshGeometry& meshGeometry, const Plane& plane);

static INLINE void SortVerts(const Plane& plane,
                             FArray<Vertex, 4>& frontVerts, u32& frontVertsCount,
                             FArray<Vertex, 4>& backVerts, u32& backVertsCount,
                             const Vertex& vert0, const Vertex& vert1,
                             const glm::vec3& pos0, const glm::vec3& pos1,
                             b8 pos0IsFront)
{
  glm::vec3 intersectionPos;
  f32 intersectionT;
  if (plane.LineIntersection(pos0, pos1, intersectionPos, intersectionT))
  {
    Vertex intersectionVert =
      {
        Math::Lerp(vert0.position, vert1.position, intersectionT),                                           // pos
        Math::Lerp(vert0.texcoord, vert1.texcoord, intersectionT), // uv
        Math::Lerp(vert0.normal, vert1.normal, intersectionT),     // normal
        glm::vec3(1.0, 0.0, 0.0) //Math::Lerp(vert0.colour, vert1.colour, intersectionT)      // colour
      };

    if (pos0IsFront)
    {
      if (frontVertsCount == 0) { frontVerts[frontVertsCount++] = vert0; }

      frontVerts[frontVertsCount++] = intersectionVert;
      backVerts[backVertsCount++]   = intersectionVert;

      if (backVertsCount < 4) { backVerts[backVertsCount++] = vert1; }
    }
    else
    {
      if (backVertsCount == 0) { backVerts[backVertsCount++] = vert0; }

      frontVerts[frontVertsCount++] = intersectionVert;
      backVerts[backVertsCount++]   = intersectionVert;

      if (frontVertsCount < 4) { frontVerts[frontVertsCount++] = vert1; }
    }
  }
  else
  {
    // As plane is not intersecting between these points, only need to check one point...
    if (pos0IsFront)
    {
      if (frontVertsCount == 0) { frontVerts[frontVertsCount++] = vert0; }
      if (frontVertsCount < 4)  { frontVerts[frontVertsCount++] = vert1; }
    }
    else
    {
      if (backVertsCount == 0) { backVerts[backVertsCount++] = vert0; }
      if (backVertsCount < 4)  { backVerts[backVertsCount++] = vert1; }
    }
  }
}

void BSPTree::BuildTree(const Mesh* mesh)
{
  // NOTE(WSWhitehouse): Working with the first mesh node only for now...

  const MeshGeometry& inputGeometry = mesh->geometryArray[0];
  const glm::mat4& transform        = mesh->nodeArray[0].transformMatrix;

  MeshGeometry meshGeometry = {};

  // Copy vertices to new geometry array
  {
    vertexCount = inputGeometry.vertexCount;
    vertices    = (Vertex*) mem_alloc((sizeof(Vertex) * vertexCount));
    mem_copy(vertices, inputGeometry.vertexArray, sizeof(Vertex) * vertexCount);

    meshGeometry.vertexCount = vertexCount;
    meshGeometry.vertexArray = vertices;
  }

  // Copy indices to new geometry array
  {
    const u64 indexCount = inputGeometry.indexCount;

    meshGeometry.indexType  = IndexType::U32_TYPE;
    meshGeometry.indexCount = indexCount;
    meshGeometry.indexArray = (u32*)mem_alloc(sizeof(u32) * indexCount);

    // NOTE(WSWhitehouse): The BSP Tree uses u32 indices. So if they are u16, when
    // copying them over they must be converted to u32. If they are already u32 they
    // can be directly copied over using mem_copy.
    switch (inputGeometry.indexType)
    {
      case IndexType::U16_TYPE:
      {
        for (u32 i = 0; i < indexCount; ++i)
        {
          meshGeometry.indexArrayU32[i] = (u32)(inputGeometry.indexArrayU16[i]);
        }
        break;
      }
      case IndexType::U32_TYPE:
      {
        mem_copy(meshGeometry.indexArray, inputGeometry.indexArray, sizeof(u32) * indexCount);
        break;
      }

      default:
      {
        LOG_FATAL("Unhandled Index Type in switch statement!");
        ABORT(ABORT_CODE_FAILURE);
      }
    }
  }

  // NOTE(WSWhitehouse): Calculate the mean as we go to reduce for-loops, this is used to center
  // the data matrix to reduce any kind of bias. Which produces a more accurate covariance matrix!
//  {
//    glm::vec3 mean = { 0.0f, 0.0f, 0.0f };
//    for (u64 i = 0; i < meshGeometry.vertexCount; ++i)
//    {
//      glm::vec3& vertPos = meshGeometry.vertexArray[i].position;
//      vertPos = transform * glm::vec4(inputGeometry.vertexArray[i].position, 1.0f);
//
//      mean += vertPos;
//    }
//
//    mean /= meshGeometry.vertexCount;
//
//    for (u64 i = 0; i < meshGeometry.vertexCount; ++i)
//    {
//      meshGeometry.vertexArray[i].position -= mean;
//    }
//  }

  nodes.Create(3);
  nodes.Add({}); // create first node

  struct QueueEntry
  {
    MeshGeometry geometry;
    u32 nodeIndex;
    u32 prevIndexCount;
    u32 depth;
  };

  std::queue<QueueEntry> queue = {};

  // Insert first queue entry
  queue.push(QueueEntry{
    .geometry        = meshGeometry,
    .nodeIndex       = 0,
    .prevIndexCount  = (u32)meshGeometry.indexCount + 1,
    .depth           = 0
  });

  while(!queue.empty())
  {
    // Get & pop the first element from the queue
    QueueEntry queueEntry = queue.front();
    queue.pop();

    Node& thisNode = nodes[queueEntry.nodeIndex];
    const u32 currentIndexCount = queueEntry.geometry.indexCount;
//    LOG_DEBUG("index count: %u", currentIndexCount);

//    if (queueEntry.depth >= 1)
//    {
////      LOG_DEBUG("Hit depth limit!");
//      thisNode.isLeaf   = true;
//
//      thisNode.indexCount = queueEntry.geometry.indexCount;
//      thisNode.indices    = queueEntry.geometry.indexArrayU32;
//      continue;
//    }

    if (currentIndexCount <= 0)
    {
//      LOG_DEBUG("Hit empty leaf!");
      thisNode.isLeaf     = true;
      thisNode.indexCount = 0;
      continue;
    }

    if (currentIndexCount >= queueEntry.prevIndexCount + 20/* && currentIndexCount <= MAX_INDICES*/)
    {
//      LOG_DEBUG("Hit index/vertex count larger or equal!");
      thisNode.isLeaf = true;

      thisNode.indexCount = queueEntry.geometry.indexCount;
      thisNode.indices    = queueEntry.geometry.indexArrayU32;
      continue;
    }

    const u32 triangleCount = queueEntry.geometry.indexCount / 3;
    if (triangleCount <= MAX_TRIANGLES)
    {
//      LOG_DEBUG("Hit max triangle count!");
      thisNode.isLeaf = true;

      thisNode.indexCount = queueEntry.geometry.indexCount;
      thisNode.indices    = queueEntry.geometry.indexArrayU32;
      continue;
    }

    if (currentIndexCount >= queueEntry.prevIndexCount)
    {
      thisNode.plane = ChooseAutoPartitioningSplitPlane(queueEntry.geometry);
    }
    else
    {
      thisNode.plane = ChooseMaxVarianceSplitPlane(queueEntry.geometry);
    }

    const FArray<MeshGeometry, 2> halfGeom = SplitMesh(queueEntry.geometry, thisNode.plane);

    // NOTE(WSWhitehouse): MUST NOT USE `thisNode` REFERENCE BEYOND THIS POINT!

    nodes[queueEntry.nodeIndex].nodePositive = nodes.Add({});
    queue.push(QueueEntry{
      .geometry       = halfGeom[0],
      .nodeIndex      = nodes[queueEntry.nodeIndex].nodePositive,
      .prevIndexCount = currentIndexCount,
      .depth          = queueEntry.depth + 1
    });

    nodes[queueEntry.nodeIndex].nodeNegative = nodes.Add({});
    queue.push(QueueEntry{
      .geometry       = halfGeom[1],
      .nodeIndex      = nodes[queueEntry.nodeIndex].nodeNegative,
      .prevIndexCount = currentIndexCount,
      .depth          = queueEntry.depth + 1
    });
  }
}

static INLINE Plane ChooseAutoPartitioningSplitPlane(const MeshGeometry& meshGeometry)
{
  const BoundingBox3D boundingBox = meshGeometry.CalculateBoundingBox();

  const glm::vec3 diff = boundingBox.maximum - boundingBox.minimum;

//  LOG_DEBUG("diff: %f, %f, %f", diff.x, diff.y, diff.z);

  // Find the largest axis of the bounding box...
  i8 largestAxis = 0;
  for (i8 i = 1; i < 3; ++i)
  {
    // NOTE(WSWhitehouse): These axis are considered the same length, randomly select one...
    if (glm::abs(diff[i] - diff[largestAxis]) <= F32_EPSILON)
    {
      largestAxis = Random::Bool() ? i : largestAxis;
      continue;
    }

    if (diff[i] > diff[largestAxis])
    {
      largestAxis = i;
    }
  }

  const glm::vec3 axis =
    {
      largestAxis == 0 ? 1.0f : 0.0f,
      largestAxis == 1 ? 1.0f : 0.0f,
      largestAxis == 2 ? 1.0f : 0.0f
    };

  f32 dotProductThreshold = 1.0f;
  u64 candidateVertIndex  = 0;

  // NOTE(WSWhitehouse): Iterating through all the triangles in the mesh, trying to find one
  // that is an ideal auto-partitioning plane. Checking how perpendicular the normal of each
  // triangle face is against the largest axis using the dot product.
  for (u64 i = 0; i < meshGeometry.indexCount; i+=3)
  {
    // Get triangle indices...
    const u32 index0 = meshGeometry.GetUniversalIndex(i + 0);
    const u32 index1 = meshGeometry.GetUniversalIndex(i + 1);
    const u32 index2 = meshGeometry.GetUniversalIndex(i + 2);

    // Get triangle vertices...
    const Vertex& vert0 = meshGeometry.vertexArray[index0];
    const Vertex& vert1 = meshGeometry.vertexArray[index1];
    const Vertex& vert2 = meshGeometry.vertexArray[index2];

    // Get triangle positions...
    const glm::vec3& pos0 = vert0.position;
    const glm::vec3& pos1 = vert1.position;
    const glm::vec3& pos2 = vert2.position;

    const Triangle triangle = {pos0, pos1, pos2};
    const glm::vec3 normal  = triangle.CalculateNormal();

    const f32 dot = glm::dot(normal, axis);

    if (dot < dotProductThreshold)
    {
      dotProductThreshold = dot;
      candidateVertIndex  = i;
    }
  }

  // Get triangle indices...
  const u32 index0 = meshGeometry.GetUniversalIndex(candidateVertIndex + 0);
  const u32 index1 = meshGeometry.GetUniversalIndex(candidateVertIndex + 1);
  const u32 index2 = meshGeometry.GetUniversalIndex(candidateVertIndex + 2);

  // Get triangle vertices...
  const Vertex& vert0 = meshGeometry.vertexArray[index0];
  const Vertex& vert1 = meshGeometry.vertexArray[index1];
  const Vertex& vert2 = meshGeometry.vertexArray[index2];

  // Get triangle positions...
  const glm::vec3& pos0 = vert0.position;
  const glm::vec3& pos1 = vert1.position;
  const glm::vec3& pos2 = vert2.position;

  const Triangle triangle = {pos0, pos1, pos2};

  return Plane
  {
    .position = triangle.CalculateCentroid(),
    .normal   = triangle.CalculateNormal()
  };
};

static INLINE Plane ChooseMaxVarianceSplitPlane(const MeshGeometry& meshGeometry)
{
  glm::mat3x3 covarianceMatrix = Eigen::CalcCovarianceMatrix3x3(meshGeometry.vertexArray, meshGeometry.vertexCount);

  FArray<f32, 3> eigenValues;
  FArray<glm::vec3, 3> eigenVectors;
  Eigen::EigenDecomposition3x3(covarianceMatrix, &eigenValues, &eigenVectors);

  for (u32 i = 0; i < 3; ++i)
  {
    eigenVectors[i] = glm::normalize(eigenVectors[i]);
  }

  glm::vec3 center = { 0.0f, 0.0f, 0.0f };
  for (u64 i = 0; i < meshGeometry.vertexCount; ++i)
  {
    center += meshGeometry.vertexArray[i].position;
  }
  center /= meshGeometry.vertexCount;

  return Plane
    {
      .position = center,
      .normal   = eigenVectors[2]
    };
}

static FArray<MeshGeometry, 2> SplitMesh(const MeshGeometry& meshGeometry, const Plane& plane)
{
  std::vector<u32> frontIndices = {};
  std::vector<u32> backIndices  = {};

  frontIndices.reserve(meshGeometry.indexCount);
  backIndices.reserve(meshGeometry.indexCount);

  for (u64 i = 0; i < meshGeometry.indexCount; i+=3)
  {
    // Get triangle indices...
    const u32 index0 = meshGeometry.GetUniversalIndex(i + 0);
    const u32 index1 = meshGeometry.GetUniversalIndex(i + 1);
    const u32 index2 = meshGeometry.GetUniversalIndex(i + 2);

    // Get triangle positions...
    const glm::vec3& pos0 = meshGeometry.vertexArray[index0].position;
    const glm::vec3& pos1 = meshGeometry.vertexArray[index1].position;
    const glm::vec3& pos2 = meshGeometry.vertexArray[index2].position;

    // Compute the distance to the plane...
    const f32 dist0 = plane.SignedDistanceFromPoint(pos0);
    const f32 dist1 = plane.SignedDistanceFromPoint(pos1);
    const f32 dist2 = plane.SignedDistanceFromPoint(pos2);

    // Compute the distance sign...
    const b8 isFront0 = dist0 > F32_EPSILON;
    const b8 isFront1 = dist1 > F32_EPSILON;
    const b8 isFront2 = dist2 > F32_EPSILON;

    // Check that all verts are in front of the plane...
    if (isFront0 && isFront1 && isFront2)
    {
      frontIndices.push_back(index0);
      frontIndices.push_back(index1);
      frontIndices.push_back(index2);
      continue;
    }

    // Check that all verts are behind the plane...
    if (!isFront0 && !isFront1 && !isFront2)
    {
      backIndices.push_back(index0);
      backIndices.push_back(index1);
      backIndices.push_back(index2);
      continue;
    }

    frontIndices.push_back(index0);
    frontIndices.push_back(index1);
    frontIndices.push_back(index2);

    backIndices.push_back(index0);
    backIndices.push_back(index1);
    backIndices.push_back(index2);
  }

  FArray<MeshGeometry, 2> outMeshes = {};
  mem_zero(&outMeshes, sizeof(FArray<MeshGeometry, 2>));

  // Front Mesh
  {
    MeshGeometry& frontGeometry = outMeshes[0];

    frontGeometry.vertexCount = meshGeometry.vertexCount;
    frontGeometry.vertexArray = meshGeometry.vertexArray;

    frontGeometry.indexType  = IndexType::U32_TYPE;
    frontGeometry.indexCount = frontIndices.size();
    frontGeometry.indexArray = mem_alloc(sizeof(u32) * frontGeometry.indexCount);
    mem_copy(frontGeometry.indexArray, frontIndices.data(), sizeof(u32) * frontGeometry.indexCount);
  }

  // Back Mesh
  {
    MeshGeometry& backGeometry = outMeshes[1];

    backGeometry.vertexCount = meshGeometry.vertexCount;
    backGeometry.vertexArray = meshGeometry.vertexArray;

    backGeometry.indexType  = IndexType::U32_TYPE;
    backGeometry.indexCount = backIndices.size();
    backGeometry.indexArray = mem_alloc(sizeof(u32) * backGeometry.indexCount);
    mem_copy(backGeometry.indexArray, backIndices.data(), sizeof(u32) * backGeometry.indexCount);
  }

  return outMeshes;
};
