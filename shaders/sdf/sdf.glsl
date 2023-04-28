#ifndef SDF_GLSL
#define SDF_GLSL

#include "../core.glsl"

float sdfMandlebulb(vec3 pos, float Speed, int Iterations, float Magnitude, float Power, float NumberOfPoints, float Multiplier)
{
  vec3 c      = pos;
  float r     = length(c);
  float dr    = 1;
  float xr    = 0;
  float theta = 0;
  float phi   = 0;

  for (int i = 0; i < Iterations && r < Magnitude; i++)
  {
    xr    = pow(r, Power);
    dr    = Multiplier * xr * dr + 1;
    theta = atan(c.y, c.x) * NumberOfPoints;
    phi   = asin(clamp(c.z / r, -1,1)) * NumberOfPoints - (FrameData.time * Speed);
    r     = xr * r;
    c     = r * vec3(cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi));

    c += pos;
    r = length(c);
  }

  return 0.35 * log(r) * r / dr;
}

vec3 Fold(vec3 p, vec3 n)
{
  return vec3(p - 2.0 * min(0.0, dot(p, n)) * n);
}

vec3 FoldX(vec3 p)
{
  return vec3(abs(p.x), p.y, p.z);
}

vec3 FoldY(vec3 p)
{
  return vec3(p.x, abs(p.y), p.z);
}

vec3 FoldZ(vec3 p)
{
  return vec3(p.x, p.y, abs(p.z));
}


float sdfSierpinskiPyramid(vec3 pos, int Iterations, float Scale)
{
  // https://github.com/ThalesII/unity-raymarcher
  for (int i = 0; i < Iterations; ++i)
  {
    pos = Fold(pos, normalize(vec3(1, 1, 0)));
    pos = Fold(pos, normalize(vec3(1, 0, 1)));
    pos = Fold(pos, normalize(vec3(0, 1, 1)));
    pos = 2.0 * pos - 0.5;
  }

  float d = max(
    max(-pos.x - pos.y - pos.z, pos.x + pos.y - pos.z),
    max(-pos.x + pos.y + pos.z, pos.x - pos.y + pos.z)
  );

  float tetra =(d - 1.0) / sqrt(3.0);
  return exp2(-Iterations) * tetra;
}

float sdfSphere(vec3 pos, vec3 center, float radius)
{
  return length(pos - center) - radius;
}

float sdfBox(vec3 pos, vec3 center, vec3 bounds)
{
  vec3 p = pos - center;
  vec3 q = abs(p) - bounds;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdfTriangle(vec3 point, vec3 a, vec3 b, vec3 c)
{
  vec3 ba = b     - a;
  vec3 pa = point - a;
  vec3 cb = c     - b;
  vec3 pb = point - b;
  vec3 ac = a     - c;
  vec3 pc = point - c;

  vec3 normal = cross(ba, ac);

  if (sign(dot(cross(ba, normal), pa)) +
      sign(dot(cross(cb, normal), pb)) +
      sign(dot(cross(ac, normal), pc)) < 2.0)
  {
    vec3 min1 = ba * clamp(dot(ba,pa) / dot(ba, ba), 0.0f ,1.0f) - pa;
    vec3 min2 = cb * clamp(dot(cb,pb) / dot(cb, cb), 0.0f, 1.0f) - pb;
    vec3 min3 = ac * clamp(dot(ac,pc) / dot(ac, ac), 0.0f, 1.0f) - pc;

    return sqrt(min(min(
                        dot(min1, min1),
                        dot(min2, min2)),
                        dot(min3, min3)));
  }

  return sqrt(dot(normal, pa) * dot(normal, pa) / dot(normal, normal));
}

#endif // SDF_GLSL
