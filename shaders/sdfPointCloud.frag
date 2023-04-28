#version 450
#include "core.glsl"
#include "pbr.glsl"
#include "sdf/sdf.glsl"
#include "sdf/operations.glsl"

layout(location = 0) in vec2 uv;        // Fragment Input
layout(location = 0) out vec4 outColor; // Fragment Output

struct NodeStack
{
  int index;
  float tNear;
  float tFar;
};

#define STACK_TYPE NodeStack
#define STACK_DEFAULT_RETURN NodeStack(0, 0.0, 0.0)
#include "containers/stack.glsl"

layout(set = 1, binding = 0) uniform SdfDataUBO
{
  mat4x4 WVP;
  mat4x4 worldMat;
  mat4x4 invWorldMat;

  vec3 boundingBox;
  uint pointCount;
} SdfData;

struct Plane
{
  vec3 position;
  vec3 normal;
};

struct BSPNode
{
  Plane plane;
  vec3 boundingBox;

  uint isLeaf;

  uint nodePos;
  uint nodeNeg;

  uint pointIndex;
  uint pointCount;
};

layout(set = 1, binding = 1) readonly buffer BSPTree    { BSPNode bspNodes[]; };
layout(set = 1, binding = 2) readonly buffer PointArray { vec3 points[];      };

struct Ray
{
  vec3 Origin;
  vec3 Direction;
  float Length;

  float CurrentDist;
};

Ray CreateCameraRay()
{
  vec3 dir = vec4(CameraData.invProjMat * vec4(uv * 2.0 - 1.0, 0.0, 1.0)).xyz;
  dir      = (CameraData.invViewMat * vec4(dir, 0.0)).xyz;

  Ray ray;
  ray.Origin    = CameraData.position;
  ray.Length    = length(dir);
  ray.Direction = normalize(dir);
  return ray;
}

#define EPSILON 0.00001

// Raymarch Settings
#define RAYMARCH_DISTANCE 300
#define RAYMARCH_RELAXATION 1.2
#define HIT_RESOLUTION 0.001
#define MAX_ITERATIONS 164

// Colours
#define OBJECT_COLOUR vec4(1.0, 1.0, 1.0, 1.0)
#define GLOW_COLOUR vec4(1.0, 0.0, 0.0, 1.0)
#define SKY_COLOUR vec4(0.75, 0.75, 0.75, 0.0)

// Ambient Occlusion Settings
#define AO_STEP_SIZE 0.2
#define AO_INTENSITY 0.25
#define AO_ITERATIONS 3

bool RayIntersectBox(Ray ray, vec3 boxPos, vec3 boxSize, out float tMin, out float tMax)
{
  tMin = EPSILON;
  tMax = RAYMARCH_DISTANCE;

  vec3 origin = ray.Origin;
  vec3 dir    = ray.Direction;

  // Calculate min and max extents
  const vec3 minExtent = -boxSize;
  const vec3 maxExtent = +boxSize;

  for (int i = 0; i < 3; ++i)
  {
    if (abs(dir[i]) < EPSILON)
    {
      // Ray is parallel to slab. No hit if origin not within slab
      if (origin[i] < minExtent[i] || origin[i] > maxExtent[i]) return false;

      continue;
    }

    // Compute intersection t value of ray with near and far plane of slab
    const float ood = 1.0f / dir[i];
    float t1 = (minExtent[i] - origin[i]) * ood;
    float t2 = (maxExtent[i] - origin[i]) * ood;

    // Make t1 be intersection with near plane, t2 with far plane
    if (t1 > t2)
    {
      float tmp = t1;
      t1 = t2;
      t2 = tmp;
    }

    // Compute the intersection of slab intersection intervals
    tMin = max(tMin, t1);
    tMax = min(tMax, t2);

    // Exit with no collision as soon as slab intersection becomes empty
    if (tMin > tMax) return false;
  }

  return true;
}

bool RayIntersectSphere(Ray ray, vec3 sphereCenter, float sphereRadius, out float tMin, out float tMax)
{
  vec3 L    = sphereCenter - ray.Origin;
  float tca = dot(L, ray.Direction);
  float d2  = dot(L, L) - tca * tca;

  if (d2 > sphereRadius * sphereRadius)
  {
    return false;
  }

  float thc = sqrt(sphereRadius * sphereRadius - d2);
  tMin = tca - thc;
  tMax = tca + thc;

  if (tMin > tMax)
  {
    float tmp = tMin;
    tMin = tMax;
    tMax = tmp;
  }

  if (tMin < 0)
  {
    tMin = tMax;
    if (tMin < 0)
    {
      return false;
    }
  }

  return true;
}

int GetNode(Ray ray)
{
  float tNear = EPSILON;
  float tFar  = RAYMARCH_DISTANCE;

  vec4 boxPos = SdfData.worldMat * vec4(0.0, 0.0, 0.0, 1.0);
  if (!RayIntersectBox(ray, boxPos.xyz, SdfData.boundingBox, tNear, tFar))
  {
    return -1;
  }

  Stack stack = StackCreate();
  int nodeIndex = 0;

  float minimumHitDist = RAYMARCH_DISTANCE;

  while(true)
  {
    BSPNode node = bspNodes[nodeIndex];

    while(node.isLeaf == 0)
    {
      const Plane plane = node.plane;
      const float denom = dot(ray.Direction, plane.normal);

      // If the denominator is zero, the ray is parallel to the plane
      //      if (abs(denom) < EPSILON)
      //      {
      //        node = bspNodes[node.nodeNeg];
      //        continue;
      //      }

      const float numer = dot(plane.position - ray.Origin, plane.normal);
      const float d = numer / denom;

      if (d <= tNear)
      {
        nodeIndex = int(node.nodeNeg);
      }
      else if(d >= tFar)
      {
        nodeIndex = int(node.nodePos);
      }
      else
      {
        NodeStack nodeStack = NodeStack(int(node.nodeNeg), d, tFar);
        StackPush(stack, nodeStack);
        nodeIndex = int(node.nodePos);
        tFar = d;
      }

      node = bspNodes[nodeIndex];
    }

    return nodeIndex;

    const uint pointCount = node.pointCount;
    if (pointCount > 0)
    {
      const uint startIndex = node.pointIndex;
      const uint endIndex   = startIndex + pointCount;
      for (uint i = startIndex; i < endIndex; i++)
      {
        float tMin, tMax;
        if (RayIntersectSphere(ray, points[i].xyz, 0.01, tMin, tMax))
        {
          minimumHitDist = min(minimumHitDist, tMin);

          if (tFar <= minimumHitDist) return nodeIndex;
        }
      }
    }

    if (StackIsEmpty(stack))
    {
      if (tFar <= minimumHitDist) return -1;

      return nodeIndex;
    }

    const NodeStack nodeStack = StackPop(stack);
    nodeIndex = nodeStack.index;
    tNear     = nodeStack.tNear;
    tFar      = nodeStack.tFar;
  }
}

float RaymarchMap(vec3 pos, Ray ray, inout vec3 colour, float minDist, int nodeIndex)
{
  colour = vec3(1.0, 0.0, 1.0);

  // NOTE(WSWhitehouse): Apply matrix transformation to the current position...
  pos = (SdfData.invWorldMat * vec4(pos.xyz, 1.0)).xyz;

//  const float bounds = sdfBox(pos, vec3(0.0, 0.0, 0.0), SdfData.boundingBox);
//  if (bounds > minDist) return bounds;

  float minFoundDist = RAYMARCH_DISTANCE;
  BSPNode node       = bspNodes[nodeIndex];

  const uint pointCount = node.pointCount;
  if (pointCount > 0)
  {
    const uint startIndex = node.pointIndex;
    const uint endIndex   = startIndex + pointCount;
    for (uint i = startIndex; i < endIndex; i++)
    {
      const float dist = sdfSphere(pos, points[i].xyz, 0.01);
      minFoundDist = min(dist, minFoundDist);

//      if (minFoundDist <= minFoundDist) return minFoundDist;
    }
  }

  return minFoundDist;
}

struct RaymarchInfo
{
  bool hit;
  float dist;
  vec3 colour;
  int iterations;
};

RaymarchInfo Raymarch(Ray ray)
{
  RaymarchInfo info;
  info.colour = OBJECT_COLOUR.rgb;
  info.hit    = false;
  info.dist   = 0.0;

  int nodeIndex = GetNode(ray);
  if (nodeIndex < 0) return info;

  for (info.iterations = 0; info.iterations < MAX_ITERATIONS; ++info.iterations)
  {
    vec3 pos   = ray.Origin + ray.Direction * info.dist;
    float dist = RaymarchMap(pos, ray, info.colour, HIT_RESOLUTION, nodeIndex);

    info.dist += dist;
    ray.CurrentDist = info.dist;

    // Reached render distance
    if (info.dist >= RAYMARCH_DISTANCE)
    {
      return info;
    }

    // Hit Something
    if (dist <= HIT_RESOLUTION)
    {
      info.hit = true;
      return info;
    }
  }

  return info;
}

void main()
{
  Ray ray = CreateCameraRay();
  RaymarchInfo info = Raymarch(ray);

  if (info.hit)
  {
    outColor = vec4(info.colour.rgb, 1.0);
  }
  else
  {
    outColor = SKY_COLOUR;
  }
}