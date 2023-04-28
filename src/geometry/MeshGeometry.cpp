#include "geometry/MeshGeometry.hpp"

// core
#include "core/Logging.hpp"

// geometry
#include "geometry/Vertex.hpp"
#include "geometry/BoundingBox3D.hpp"

// threading
#include "threading/JobSystem.hpp"

// Forward Declarations
static void CalculateBoundingBoxAsyncU32(const u32* indexArr, const Vertex* vertexArr, const glm::mat4& transform,
                                         u32 startIndex, u32 blockSize, BoundingBox3D* boundingBox);
static void CalculateBoundingBoxAsyncU16(const u16* indexArr, const Vertex* vertexArr, const glm::mat4& transform,
                                         u32 startIndex, u32 blockSize, BoundingBox3D* boundingBox);

BoundingBox3D MeshGeometry::CalculateBoundingBox(const glm::mat4& transform) const
{
  constexpr const u32 MinIndicesPerThread = 200;
  const u32 numThreads = (indexCount + MinIndicesPerThread) / MinIndicesPerThread;
  const u32 numJobs    = numThreads - 1;
  const u32 blockSize  = indexCount / numThreads;

  std::vector<BoundingBox3D> results(numJobs);
  std::vector<JobSystem::JobHandle> jobs(numJobs);

  u32 startIndex = 0;
  for (u32 i = 0; i < numJobs; ++i)
  {
    BoundingBox3D* resultsPtr = &results[i];
    JobSystem::WorkFuncPtr lambda;

    switch (indexType)
    {
      case IndexType::U16_TYPE:
      {
        lambda = [=, &transform, this]
        { CalculateBoundingBoxAsyncU16(indexArrayU16, vertexArray, transform, startIndex, blockSize, resultsPtr); };
        break;
      }
      case IndexType::U32_TYPE:
      {
        lambda = [=, &transform, this]
        { CalculateBoundingBoxAsyncU32(indexArrayU32, vertexArray, transform, startIndex, blockSize, resultsPtr); };
        break;
      }
    }

    jobs[i] = JobSystem::SubmitJob(lambda);
    startIndex += blockSize;
  }

  // NOTE(WSWhitehouse): This isn't calling the function asynchronously... duh.
  BoundingBox3D boundingBox;
  const u64 finalBlockSize = indexCount - startIndex;

  switch (indexType)
  {
    case IndexType::U16_TYPE:
    {
      CalculateBoundingBoxAsyncU16(indexArrayU16, vertexArray, transform, startIndex, finalBlockSize, &boundingBox);
      break;
    }
    case IndexType::U32_TYPE:
    {
      CalculateBoundingBoxAsyncU32(indexArrayU32, vertexArray, transform, startIndex, finalBlockSize, &boundingBox);
      break;
    }
  }

  for (u32 i = 0; i < numJobs; ++i)
  {
    jobs[i].WaitUntilComplete();

    boundingBox.EncapsulatePoint(results[i].minimum);
    boundingBox.EncapsulatePoint(results[i].maximum);
  }

  return boundingBox;
}

static void CalculateBoundingBoxAsyncU32(const u32* indexArr, const Vertex* vertexArr, const glm::mat4& transform,
                                      u32 startIndex, u32 blockSize, BoundingBox3D* boundingBox)
{
  boundingBox->minimum = glm::vec3(F32_MAX, F32_MAX, F32_MAX);
  boundingBox->maximum = glm::vec3(F32_MIN, F32_MIN, F32_MIN);

  const u64 endIndex = startIndex + blockSize;
  for (u64 i = startIndex; i < endIndex; i++)
  {
    const u32 index = indexArr[i];
    const glm::vec3& vertex = transform * glm::vec4(vertexArr[index].position, 1.0f);
    boundingBox->EncapsulatePoint(vertex);
  }
}

static void CalculateBoundingBoxAsyncU16(const u16* indexArr, const Vertex* vertexArr, const glm::mat4& transform,
                                         u32 startIndex, u32 blockSize, BoundingBox3D* boundingBox)
{
  boundingBox->minimum = glm::vec3(F32_MAX, F32_MAX, F32_MAX);
  boundingBox->maximum = glm::vec3(F32_MIN, F32_MIN, F32_MIN);

  const u64 endIndex = startIndex + blockSize;
  for (u64 i = startIndex; i < endIndex; i++)
  {
    const u16 index = indexArr[i];
    const glm::vec3& vertex = transform * glm::vec4(vertexArr[index].position, 1.0f);
    boundingBox->EncapsulatePoint(vertex);
  }
}


MeshGeometry::ExtremeVertexIndices MeshGeometry::FindExtremeVertices(const glm::mat4& transform) const
{
  ExtremeVertexIndices extremeIndices = {};
  mem_zero(&extremeIndices, sizeof(ExtremeVertexIndices));

  glm::vec3 posXPosition = transform * glm::vec4(vertexArray[extremeIndices.posX].position, 1.0f);
  glm::vec3 negXPosition = transform * glm::vec4(vertexArray[extremeIndices.negX].position, 1.0f);
  glm::vec3 posYPosition = transform * glm::vec4(vertexArray[extremeIndices.posY].position, 1.0f);
  glm::vec3 negYPosition = transform * glm::vec4(vertexArray[extremeIndices.negY].position, 1.0f);
  glm::vec3 posZPosition = transform * glm::vec4(vertexArray[extremeIndices.posZ].position, 1.0f);
  glm::vec3 negZPosition = transform * glm::vec4(vertexArray[extremeIndices.negZ].position, 1.0f);

  for (u64 i = 1; i < vertexCount; ++i)
  {
    const glm::vec3 vertPosition = transform * glm::vec4(vertexArray[i].position, 1.0f);

    // +X Axis
    if (posXPosition.x < vertPosition.x)
    {
      extremeIndices.posX = i;
      posXPosition = vertPosition;
    }

    // -X Axis
    if (negXPosition.x > vertPosition.x)
    {
      extremeIndices.negX = i;
      negXPosition = vertPosition;
    }

    // +Y Axis
    if (posYPosition.y < vertPosition.y)
    {
      extremeIndices.posY = i;
      posYPosition = vertPosition;
    }

    // -Y Axis
    if (negYPosition.y > vertPosition.y)
    {
      extremeIndices.negY = i;
      negYPosition = vertPosition;
    }

    // +Z Axis
    if (posZPosition.z < vertPosition.z)
    {
      extremeIndices.posZ = i;
      posZPosition = vertPosition;
    }

    // -Z Axis
    if (negZPosition.z > vertPosition.z)
    {
      extremeIndices.negZ = i;
      negZPosition = vertPosition;
    }
  }

  return extremeIndices;
}