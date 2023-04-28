#version 450
#include "../core.glsl"
#include "../pbr.glsl"
#include "../sdf/sdf.glsl"
#include "../sdf/operations.glsl"

layout(set = 1, binding = 0) uniform UBVoxelData
{
// Matrices
  mat4 WVP;
  mat4 worldMat;
  mat4 invWorldMat;

// Raymarch Data
  uvec3 cellCount;
  vec3 gridExtents;
  vec3 twist;
  vec4 sphere;
  float voxelGridScale;
  float blend;
  bool showBounds;
} VoxelData;

layout(set = 1, binding = 1) uniform sampler3D voxelGrid;

// Fragment Output
layout(location = 0) out vec4 outColor;

// Vertex Output
layout(location = 0) in vec2 uv;

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
#define MAX_ITERATIONS (164 * 10)

// Colours
#define OBJECT_COLOUR vec4(1.0, 1.0, 1.0, 1.0)
#define GLOW_COLOUR vec4(1.0, 0.0, 0.0, 1.0)
#define SKY_COLOUR vec4(0.75, 0.75, 0.75, 0.0)

// Ambient Occlusion Settings
#define AO_STEP_SIZE 0.2
#define AO_INTENSITY 0.25
#define AO_ITERATIONS 3

float VoxelDistance(vec3 pos, float minDist)
{
  // NOTE(WSWhitehouse): Apply matrix transformation to the current position...
  pos = (VoxelData.invWorldMat * vec4(pos.xyz, 1.0)).xyz;
  if (VoxelData.twist.x > 0.000001) { pos = sdf_TwistX(pos, VoxelData.twist.x); }
  if (VoxelData.twist.y > 0.000001) { pos = sdf_TwistY(pos, VoxelData.twist.y); }
  if (VoxelData.twist.z > 0.000001) { pos = sdf_TwistZ(pos, VoxelData.twist.z); }

  const vec3 gridSize     = VoxelData.gridExtents;
  const vec3 gridHalfSize = gridSize * 0.5;
  const vec3 center       = pos + gridHalfSize;

  float bounds = sdfBox(pos, vec3(0.0, 0.0, 0.0), gridHalfSize);
  if (VoxelData.showBounds) return bounds;

  // NOTE(WSWhitehouse): Scales the position to be between [0,1] so its a valid tex coord.
  const vec3 texPos     = (center / gridSize);
  const float voxel     = texture(voxelGrid, texPos).r;
  const float voxelDist = voxel / VoxelData.voxelGridScale;

//  if (bounds <= minDist)
//  {
//    return voxelDist;
//    //    return sdf_SineWave(dist, vec3(1.0, 0.0, 1.0), 0.5, 1, 0.5);
//    //    return sdf_Displacement(pos, dist, vec3(10.0,10.0,10.0) * FrameData.sinTime) / 3;
//  }

  return max(0.0, bounds) + voxelDist;
}

float RaymarchMap(vec3 pos, inout vec3 colour, float minDist)
{
  const vec3 voxelColour  = vec3(1.0, 1.0, 1.0);
  const vec3 sphereColour = vec3(1.0, 0.1, 0.1);

  const float voxelDist  = VoxelDistance(pos, minDist);
  const float sphereDist = length(VoxelData.sphere.xyz - pos) - VoxelData.sphere.w;

  const float h = CLAMP01(0.5 + 0.5 * (sphereDist - voxelDist) / VoxelData.blend);
  colour =  mix(sphereColour, voxelColour, h) - VoxelData.blend * h * (1.0 - h);
  const float dist = mix(sphereDist, voxelDist, h) - VoxelData.blend * h * (1.0 - h);
  return dist;

//  return min(voxelDist, sphereDist);
}

// NOTE(WSWhitehouse): A simple overload for the RaymarchMap func,
// so code that doesn't care about minimum distance can call it.
float RaymarchMap(vec3 pos)
{
  vec3 _unused;
  return RaymarchMap(pos, _unused, HIT_RESOLUTION);
}

// NOTE(WSWhitehouse): A simple overload for the RaymarchMap func,
// so code that doesn't care about minimum distance can call it.
float RaymarchMap(vec3 pos, float minDist)
{
  vec3 _unused;
  return RaymarchMap(pos, _unused, minDist);
}

vec3 GetObjectNormal(vec3 pos)
{
  const float dist  = RaymarchMap(pos);
  const vec2 offset = vec2(0.001, 0.0);
  const vec3 normal = vec3(
    RaymarchMap(pos + offset.xyy) - dist,
    RaymarchMap(pos + offset.yxy) - dist,
    RaymarchMap(pos + offset.yyx) - dist
  );

  return normalize(normal);
}

float AmbientOcclusion(vec3 pos, vec3 normal)
{
  float ao = 0.0;
  for (int i = 1; i <= AO_ITERATIONS; i++)
  {
    float dist = AO_STEP_SIZE * i;
    ao += max(0.0, (dist - RaymarchMap(pos + normal * dist)) / dist);
  }

  return (1.0 - ao * AO_INTENSITY);
}

vec4 CalculateColour(vec3 pos, vec3 normal, vec3 objColour)
{
  vec3 pbrColour = PBRCalculateColour(pos, objColour, normal);
  pbrColour *= AmbientOcclusion(pos, normal);

  return vec4(pbrColour, 1.0);
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

  for (info.iterations = 0; info.iterations < MAX_ITERATIONS; ++info.iterations)
  {
    vec3 pos   = ray.Origin + ray.Direction * info.dist;
    float dist = RaymarchMap(pos, info.colour, HIT_RESOLUTION);

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

RaymarchInfo RaymarchRelaxed(Ray ray)
{
  RaymarchInfo info;
  info.hit  = false;
  info.dist = 0.0;

  float candidateError        = RAYMARCH_DISTANCE;
  float candidateDistTraveled = info.dist;

  // NOTE(WSWhitehouse): Over-Relaxation Sphere Tracing (Paper: Enhanced Sphere Tracing)
  float relaxOmega = RAYMARCH_RELAXATION;
  float prevRadius = 0;
  float stepLength = 0;
  float funcSign   = RaymarchMap(ray.Origin, HIT_RESOLUTION) < 0 ? -1 : +1;

  for (info.iterations = 0; info.iterations < MAX_ITERATIONS; ++info.iterations)
  {
    vec3 pos   = ray.Origin + ray.Direction * info.dist;
    float dist = RaymarchMap(pos, info.colour, prevRadius);

    float signedRadius = funcSign * dist;
    float radius       = abs(signedRadius);

    bool sorFail = relaxOmega > 1 && (radius + prevRadius) < stepLength;
    prevRadius   = radius;

    if (sorFail)
    {
      stepLength -= relaxOmega * stepLength;
      info.dist  += stepLength;
      relaxOmega  = 1;
      continue;
    }

    stepLength = signedRadius * relaxOmega;
    info.dist += stepLength;

    // Reached render distance
    if (info.dist > RAYMARCH_DISTANCE)
    {
      return info;
    }

    float error = radius / info.dist;
    if (error < candidateError)
    {
      candidateDistTraveled = info.dist;
      candidateError        = error;

      // Hit Something
      if (error < HIT_RESOLUTION)
      {
        info.hit = true;
        return info;
      }
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
    const vec3 pos    = ray.Origin + ray.Direction * info.dist;
    const vec3 normal = GetObjectNormal(pos);
    const vec4 colour = CalculateColour(pos, normal, info.colour);
    outColor = vec4(colour.rgb, 1.0);
    return;

    // NOTE(WSWhitehouse): Applying glow based on number of iterations
    // https://adrianb.io/2016/10/01/raymarching.html
    const float perf = float(info.iterations) / float(MAX_ITERATIONS);
    outColor = mix(colour, GLOW_COLOUR, perf);
  }
  else
  {
    outColor = SKY_COLOUR;
  }
}