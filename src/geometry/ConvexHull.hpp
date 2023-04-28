#ifndef SNOWFLAKE_CONVEX_HULL_HPP
#define SNOWFLAKE_CONVEX_HULL_HPP

#include "pch.hpp"

// Forward Declarations
struct MeshGeometry;

namespace ConvexHull
{

  MeshGeometry ComputeQuickHull3D(const MeshGeometry& inMeshGeometry);

} // namespace ConvexHull

#endif //SNOWFLAKE_CONVEX_HULL_HPP
