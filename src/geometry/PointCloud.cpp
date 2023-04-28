#include "PointCloud.hpp"

// core
#include "core/Random.hpp"

// geometry
#include "geometry/Mesh.hpp"
#include "geometry/MeshGeometry.hpp"
#include "geometry/BoundingBox3D.hpp"
#include "geometry/Vertex.hpp"

// threading
#include "threading/JobSystem.hpp"

#define POINT_COUNT 50

void PointCloud::GenerateFromMesh(const Mesh* mesh)
{
  const MeshGeometry& geometry = mesh->geometryArray[0];

  points     = (glm::vec3*)mem_alloc(sizeof(glm::vec3) * POINT_COUNT * (geometry.indexCount / 3));
  pointCount = 0;

  for (u32 triIndex = 0; triIndex < geometry.indexCount; triIndex+=3)
  {
    // Get triangle indices...
    const u32 index0 = geometry.GetUniversalIndex(triIndex + 0);
    const u32 index1 = geometry.GetUniversalIndex(triIndex + 1);
    const u32 index2 = geometry.GetUniversalIndex(triIndex + 2);

    // Get triangle vertices...
    const Vertex& vert0 = geometry.vertexArray[index0];
    const Vertex& vert1 = geometry.vertexArray[index1];
    const Vertex& vert2 = geometry.vertexArray[index2];

    // Get triangle positions...
    const glm::vec3& pos0 = vert0.position;
    const glm::vec3& pos1 = vert1.position;
    const glm::vec3& pos2 = vert2.position;

    for (u32 i = 0; i < POINT_COUNT; ++i)
    {
      const f32 rand0 = Random::Range(0.0f, 1.0f + F32_EPSILON);
      const f32 rand1 = Random::Range(0.0f, 1.0f + F32_EPSILON);

      points[pointCount] = (1 - glm::sqrt(rand0)) * pos0 + (glm::sqrt(rand0) * (1 - rand1)) * pos1 + (glm::sqrt(rand0) * rand1) * pos2;
      pointCount++;
    }
  }
}

static void CalculateBoundingBoxAsync(const glm::vec3* pointsArray, const glm::mat4x4& transform,
                                      u32 startIndex, u32 blockSize, BoundingBox3D* boundingBox)
{
  boundingBox->minimum = glm::vec3(F32_MAX, F32_MAX, F32_MAX);
  boundingBox->maximum = glm::vec3(F32_MIN, F32_MIN, F32_MIN);

  const u64 endIndex = startIndex + blockSize;
  for (u64 i = startIndex; i < endIndex; i++)
  {
    const glm::vec3& point = transform * glm::vec4(pointsArray[i], 1.0f);
    boundingBox->EncapsulatePoint(point);
  }
}

BoundingBox3D PointCloud::CalculateBoundingBox(const glm::mat4x4& transform) const
{
  constexpr const u32 MinPointsPerThread = 200;
  const u32 numThreads = (pointCount + MinPointsPerThread) / MinPointsPerThread;
  const u32 numJobs    = numThreads - 1;
  const u32 blockSize  = pointCount / numThreads;

  DArray<BoundingBox3D> results     = {};
  DArray<JobSystem::JobHandle> jobs = {};
  results.Create(numJobs);
  jobs.Create(numJobs);

  u32 startIndex = 0;
  for (u32 i = 0; i < numJobs; ++i)
  {
    BoundingBox3D* resultsPtr = &results[i];
    JobSystem::WorkFuncPtr lambda= [=, &transform, this]
    { CalculateBoundingBoxAsync(points, transform, startIndex, blockSize, resultsPtr); };

    jobs[i] = JobSystem::SubmitJob(lambda);
    startIndex += blockSize;
  }

  // NOTE(WSWhitehouse): This isn't calling the function asynchronously... duh.
  BoundingBox3D boundingBox;
  const u64 finalBlockSize = pointCount - startIndex;
  CalculateBoundingBoxAsync(points, transform, startIndex, finalBlockSize, &boundingBox);

  for (u32 i = 0; i < numJobs; ++i)
  {
    jobs[i].WaitUntilComplete();

    boundingBox.EncapsulatePoint(results[i].minimum);
    boundingBox.EncapsulatePoint(results[i].maximum);
  }

  results.Destroy();
  jobs.Destroy();

  return boundingBox;
}