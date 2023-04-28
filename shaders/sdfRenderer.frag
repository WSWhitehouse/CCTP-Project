#version 450
#include "core.glsl"
#include "pbr.glsl"
#include "sdf/sdf.glsl"
#include "sdf/operations.glsl"

layout(location = 0) in vec2 uv;        // Fragment Input
layout(location = 0) out vec4 outColor; // Fragment Output

layout(set = 1, binding = 0) uniform SdfDataUBO
{
  mat4x4 WVP;
  mat4x4 worldMat;
  mat4x4 invWorldMat;

  vec3 boundingBoxExtents;

  uint indexCount;
} SdfData;

struct Plane
{
  vec3 position;
  vec3 normal;
};

#define MAX_TRIANGLES 75U
#define MAX_INDICES (MAX_TRIANGLES * 3U)
#define EPSILON 0.00001

struct NodeStack
{
  int index;
  float tNear;
  float tFar;
};

#define STACK_TYPE NodeStack
#define STACK_DEFAULT_RETURN NodeStack(0, 0.0, 0.0)
#include "containers/stack.glsl"

struct BSPNode
{
  Plane plane;

  uint isLeaf;

  uint nodePos;
  uint nodeNeg;

  uint indexStart;
  uint indexCount;
};

struct Vertex
{
  vec3 position;
  vec3 normal;
  vec3 colour;
  vec2 texcoord;
};

layout(set = 1, binding = 1, std430) readonly buffer BSPTree     { BSPNode bspNodes[]; };
layout(set = 1, binding = 2, std430) readonly buffer IndexArray  { uint indices[];     };
layout(set = 1, binding = 3, std430) readonly buffer VertexArray { Vertex vertices[];  };

struct Ray
{
  vec3 Origin;
  vec3 Direction;
  float Length;
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

int GetNode(Ray ray)
{
  float tNear = EPSILON;
  float tFar  = RAYMARCH_DISTANCE;

  vec4 boxPos = SdfData.worldMat * vec4(0.0, 0.0, 0.0, 1.0);
  if (!RayIntersectBox(ray, boxPos.xyz, SdfData.boundingBoxExtents, tNear, tFar))
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



    const uint pointCount = node.indexCount;
    if (pointCount > 0)
    {
      return nodeIndex;
    }
//      const uint startIndex = node.pointIndex;
//      const uint endIndex   = startIndex + pointCount;
//      for (uint i = startIndex; i < endIndex; i++)
//      {
//        float tMin, tMax;
//        if (RayIntersectSphere(ray, points[i].xyz, 0.01, tMin, tMax))
//        {
//          minimumHitDist = min(minimumHitDist, tMin);
//
//          if (tFar <= minimumHitDist) return nodeIndex;
//        }
//      }
//    }

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

float sdfTriangle2(vec3 point, vec3 a, vec3 b, vec3 c, vec3 normal)
{
  const vec3 ba = b     - a;
  const vec3 pa = point - a;
  const vec3 cb = c     - b;
  const vec3 pb = point - b;
  const vec3 ac = a     - c;
  const vec3 pc = point - c;

  const float paNormalDot = dot(normal, pa);

  if (sign(dot(cross(ba, normal), pa)) +
  sign(dot(cross(cb, normal), pb)) +
  sign(dot(cross(ac, normal), pc)) < 2.0)
  {
    const vec3 min1 = ba * CLAMP01(dot(ba,pa) / dot(ba, ba)) - pa;
    const vec3 min2 = cb * CLAMP01(dot(cb,pb) / dot(cb, cb)) - pb;
    const vec3 min3 = ac * CLAMP01(dot(ac,pc) / dot(ac, ac)) - pc;

    float dist = sqrt(min(min(dot(min1, min1),
    dot(min2, min2)),
    dot(min3, min3)));

    dist = (paNormalDot < EPSILON) ? dist : -dist;
    return dist;
  }

  float dist = sqrt(paNormalDot * paNormalDot / dot(normal, normal));
  dist = (paNormalDot < EPSILON) ? dist : -dist;

  return dist;
}

float RaymarchMap(Ray ray, vec3 pos, inout vec3 colour, float minDist, int nodeIndex)
{
  colour = vec3(1.0, 1.0, 1.0);

  // NOTE(WSWhitehouse): Apply matrix transformation to the current position...
  pos = (SdfData.invWorldMat * vec4(pos.xyz, 1.0)).xyz;

  const float bounds = sdfBox(pos, vec3(0.0, 0.0, 0.0), SdfData.boundingBoxExtents);
  if (bounds > minDist) return bounds;

  Stack stack = StackCreate();
  float dist = RAYMARCH_DISTANCE;
  nodeIndex = 0;

  while(true)
  {
    BSPNode node = bspNodes[nodeIndex];

    while(node.isLeaf == 0)
    {
      const vec3 planeDiff  = pos - node.plane.position;
      const float planeDist = dot(normalize(node.plane.normal), planeDiff);

      if (abs(planeDist) < dist)
      {
        if (planeDist > 0.0)
        {
          const NodeStack nodeStack = NodeStack(int(node.nodeNeg), 0, 0);
          StackPush(stack, nodeStack);
          nodeIndex = int(node.nodePos);
        }
        else
        {
          const NodeStack nodeStack = NodeStack(int(node.nodePos), 0, 0);
          StackPush(stack, nodeStack);
          nodeIndex = int(node.nodeNeg);
        }
      }
      else
      {
        if (StackIsEmpty(stack))
        {
          return dist;
        }

        const NodeStack nodeStack = StackPop(stack);
        nodeIndex = nodeStack.index;
      }

      node = bspNodes[nodeIndex];
    }

    const uint indexCount = node.indexCount;
    if (indexCount != 0)
    {
      const uint startIndex = node.indexStart;
      const uint endIndex   = startIndex + indexCount;
      for (uint i = startIndex; i < endIndex; i+=3)
      {
        const uint index0 = indices[i + 0];
        const uint index1 = indices[i + 1];
        const uint index2 = indices[i + 2];

        const vec3 vert0  = vertices[index0].position;
        const vec3 vert1  = vertices[index1].position;
        const vec3 vert2  = vertices[index2].position;
        const vec3 normal = normalize(cross((vert1 - vert0), (vert0 - vert2)));

        const float thisTriDist = sdfTriangle2(pos, vert0, vert1, vert2, normal);
        if (abs(thisTriDist) < abs(dist))
        {
          dist = thisTriDist;
        }

      }
    }

    if (StackIsEmpty(stack))
    {
      return dist;
    }

    const NodeStack nodeStack = StackPop(stack);
    nodeIndex = nodeStack.index;
  }



//  const uint startIndex = bspNodes[nodeIndex].indexStart;
//  const uint endIndex   = startIndex + bspNodes[nodeIndex].indexCount;
//  const uint indexCount = endIndex - startIndex;
//
//  float minTriDist = RAYMARCH_DISTANCE;
//
//  if (indexCount <= 0) return minTriDist;
//
//  for (uint i = startIndex; i < endIndex; i+=3)
//  {
//    const uint index0 = indices[i + 0];
//    const uint index1 = indices[i + 1];
//    const uint index2 = indices[i + 2];
//
//    const vec3 vert0 = vertices[index0].position;
//    const vec3 vert1 = vertices[index1].position;
//    const vec3 vert2 = vertices[index2].position;
//
//    const float thisTriDist = sdfTriangle(pos, vert0, vert1, vert2);
//
//    if (abs(thisTriDist) < abs(minTriDist))
//    {
//      minTriDist = thisTriDist;
//    }
//
//    // NOTE(WSWhitehouse): No need to check other triangles if this point
//    // has already hit the minimum distance to a triangle!
//    if (thisTriDist < minDist) return minTriDist;
//  }
//
//  return minTriDist;

//  float tNear = EPSILON;
//  float tFar  = RAYMARCH_DISTANCE;
//
//  vec4 boxPos = SdfData.worldMat * vec4(0.0, 0.0, 0.0, 1.0);
//  if (!RayIntersectBox(ray, boxPos.xyz, SdfData.boundingBoxExtents, tNear, tFar))
//  {
//    return RAYMARCH_DISTANCE;
//  }
//
//  Stack stack = StackCreate();
//  nodeIndex = 0;
//
//  float minimumHitDist = RAYMARCH_DISTANCE;
//
//  while(true)
//  {
//    BSPNode node = bspNodes[nodeIndex];
//
//    while(node.isLeaf == 0)
//    {
//      const Plane plane = node.plane;
//      const float denom = dot(ray.Direction, plane.normal);
//      const float numer = dot(plane.position - ray.Origin, plane.normal);
//      const float d = numer / denom;
//
//      if (d <= tNear)
//      {
//        nodeIndex = int(node.nodeNeg);
//      }
//      else if(d >= tFar)
//      {
//        nodeIndex = int(node.nodePos);
//      }
//      else
//      {
//        NodeStack nodeStack = NodeStack(int(node.nodeNeg), d, tFar);
//        StackPush(stack, nodeStack);
//        nodeIndex = int(node.nodePos);
//        tFar = d;
//      }
//
//      node = bspNodes[nodeIndex];
//    }
//
////    const vec3 planeDiff  = pos - bspNodes[nodeIndex].plane.position;
////    const float planeDist = dot(bspNodes[nodeIndex].plane.normal, planeDiff);
//
//    const uint indexCount = node.indexCount;
//    if (indexCount != 0)
//    {
//      const uint startIndex = node.indexStart;
//      const uint endIndex   = startIndex + indexCount;
//      for (uint i = startIndex; i < endIndex; i+=3)
//      {
//        const uint index0 = indices[i + 0];
//        const uint index1 = indices[i + 1];
//        const uint index2 = indices[i + 2];
//
//        const vec3 vert0 = vertices[index0].position;
//        const vec3 vert1 = vertices[index1].position;
//        const vec3 vert2 = vertices[index2].position;
//
//        const float thisTriDist = sdfTriangle(pos, vert0, vert1, vert2);
//        minimumHitDist = min(minimumHitDist, thisTriDist);
//
//        // NOTE(WSWhitehouse): No need to check other triangles if this point
//        // has already hit the minimum distance to a triangle!
//        if (minimumHitDist < minDist) return minimumHitDist;
//      }
//    }
//
//    if (StackIsEmpty(stack))
//    {
//      return minimumHitDist;
//    }
//
//    const NodeStack nodeStack = StackPop(stack);
//    nodeIndex = nodeStack.index;
//    tNear     = nodeStack.tNear;
//    tFar      = nodeStack.tFar;
//  }

//  nodeIndex = 0;
//  while(true)
//  {
//    if (bspNodes[nodeIndex].isLeaf == 1)
//    {
//      const uint startIndex = bspNodes[nodeIndex].indexStart;
//      const uint endIndex   = startIndex + bspNodes[nodeIndex].indexCount;
//      const uint indexCount = endIndex - startIndex;
//
//      if (indexCount <= 0)
//      {
//        const vec3 planeDiff = pos - bspNodes[nodeIndex].plane.position;
//        return dot(bspNodes[nodeIndex].plane.normal, planeDiff);
//      }
//
//      float minTriDist = RAYMARCH_DISTANCE;
//
//      for (uint i = startIndex; i < endIndex; i+=3)
//      {
//        const uint index0 = indices[i + 0];
//        const uint index1 = indices[i + 1];
//        const uint index2 = indices[i + 2];
//
//        const vec3 vert0 = vertices[index0].position;
//        const vec3 vert1 = vertices[index1].position;
//        const vec3 vert2 = vertices[index2].position;
//
//        const float thisTriDist = sdfTriangle(pos, vert0, vert1, vert2);
//
//        if (thisTriDist < minTriDist)
//        {
//          minTriDist = thisTriDist;
//        }
//
//        // NOTE(WSWhitehouse): No need to check other triangles if this point
//        // has already hit the minimum distance to a triangle!
//        if (thisTriDist < minDist) return minTriDist;
//      }
//
//      return minTriDist;
//    }
//
//    const vec3 planeDiff  = pos - bspNodes[nodeIndex].plane.position;
//    const float planeDist = dot(bspNodes[nodeIndex].plane.normal, planeDiff);
//
//    if (planeDist >= 0.0f)
//    {
//      nodeIndex = int(bspNodes[nodeIndex].nodePos);
//    }
//    else
//    {
//      nodeIndex = int(bspNodes[nodeIndex].nodeNeg);
//    }
//  }
}

// NOTE(WSWhitehouse): A simple overload for the RaymarchMap func,
// so code that doesn't care about minimum distance can call it.
float RaymarchMap(Ray ray, vec3 pos, int nodeIndex)
{
  vec3 _unused;
  return RaymarchMap(ray, pos, _unused, HIT_RESOLUTION, nodeIndex);
}

// NOTE(WSWhitehouse): A simple overload for the RaymarchMap func,
// so code that doesn't care about minimum distance can call it.
float RaymarchMap(Ray ray, vec3 pos, float minDist, int nodeIndex)
{
  vec3 _unused;
  return RaymarchMap(ray, pos, _unused, minDist, nodeIndex);
}

vec3 GetObjectNormal(Ray ray, vec3 pos, int nodeIndex)
{
  const float dist  = RaymarchMap(ray, pos, nodeIndex);
  const vec2 offset = vec2(0.001, 0.0);
  const vec3 normal = vec3(
    RaymarchMap(ray, pos + offset.xyy, nodeIndex) - dist,
    RaymarchMap(ray, pos + offset.yxy, nodeIndex) - dist,
    RaymarchMap(ray, pos + offset.yyx, nodeIndex) - dist
  );

  return normalize(normal);
}

float AmbientOcclusion(Ray ray, vec3 pos, vec3 normal, int nodeIndex)
{
  float ao = 0.0;
  for (int i = 1; i <= AO_ITERATIONS; i++)
  {
    float dist = AO_STEP_SIZE * i;
    ao += max(0.0, (dist - RaymarchMap(ray, pos + normal * dist, nodeIndex)) / dist);
  }

  return (1.0 - ao * AO_INTENSITY);
}

vec4 CalculateColour(Ray ray, vec3 pos, int nodeIndex, vec3 normal, vec3 objColour)
{
  vec3 pbrColour = PBRCalculateColour(pos, objColour, normal);
  pbrColour *= AmbientOcclusion(ray, pos, normal, nodeIndex);
  return vec4(pbrColour, 1.0);
}

struct RaymarchInfo
{
  bool hit;
  float dist;
  vec3 colour;
  int iterations;
};

RaymarchInfo Raymarch(Ray ray, int nodeIndex)
{
  RaymarchInfo info;
  info.colour = OBJECT_COLOUR.rgb;
  info.hit    = false;
  info.dist   = 0.0;

  if (nodeIndex < 0) return info;

  for (info.iterations = 0; info.iterations < MAX_ITERATIONS; ++info.iterations)
  {
    vec3 pos   = ray.Origin + ray.Direction * info.dist;
    float dist = RaymarchMap(ray, pos, info.colour, HIT_RESOLUTION, nodeIndex);

    info.dist += dist;

    // Reached render distance
    if (info.dist > RAYMARCH_DISTANCE)
    {
      return info;
    }

    // Hit Something
    if (dist < HIT_RESOLUTION)
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

  const int nodeIndex = GetNode(ray);
  RaymarchInfo info = Raymarch(ray, nodeIndex);

  if (info.hit)
  {
    const vec3 pos    = ray.Origin + ray.Direction * info.dist;
    const vec3 normal = GetObjectNormal(ray, pos, nodeIndex);
    const vec4 colour = CalculateColour(ray, pos, nodeIndex, normal, info.colour);
    outColor = vec4(colour.rgb, 1.0);
  }
  else
  {
    outColor = SKY_COLOUR;
  }
}