#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#define PI 3.14159265
#include "common.h"

layout(location = 0) out vec4 color;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1) uniform sampler2D depthMap;

layout(push_constant) uniform params_t
{
  mat4 projInv;
  mat4 viewInv;
} params;

float getDepth()
{
  float z = textureLod(depthMap, surf.texCoord, 0).r;
  vec4 pos = params.viewInv * params.projInv * vec4(surf.texCoord * 2.0 - vec2(1.0), z, 1.0);
  pos /= pos.w;
  return length(Params.camPos.xyz - pos.xyz);
}

vec3 getViewDir()
{
  vec4 viewDir = vec4(surf.texCoord * 2.0 - vec2(1.0), 0, 0);
  viewDir = params.projInv * viewDir + vec4(0, 0, -1, 0);
  viewDir = params.viewInv * viewDir;
  return normalize(viewDir.xyz);
}

float sdfSphere(vec3 p)
{
  vec3 SPHERE_POS = vec3(0);
  float R = 5.0;
  return length(SPHERE_POS - p) - R;
}

float sdfBox(vec3 p, vec3 b)
{
  vec3 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdfCone(vec3 p, float tanh, float h)
{
  vec2 q = h * vec2(tanh, -1.0);    
  vec2 w = vec2(length(p.xz), p.y);
  vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
  vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
  float k = sign(q.y);
  float d = min(dot(a, a), dot(b, b));
  float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
  return sqrt(d) * sign(s);
}

float sdfCylinder(vec3 p, float h, float r)
{
  vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r,h);
  return min(max(d.x,d.y), 0.0) + length(max(d, 0.0));
}

float sdfStar2d(vec2 p, float r, float rf)
{
  const vec2 k1 = vec2(0.809016994375, -0.587785252292);
  const vec2 k2 = vec2(-k1.x, k1.y);
  p.x = abs(p.x);
  p -= 2.0 * max(dot(k1, p), 0.0) * k1;
  p -= 2.0 * max(dot(k2, p), 0.0) * k2;
  p.x = abs(p.x);
  p.y -= r;
  vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0, 1);
  float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
  return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
}

float sdfStar(vec3 p, float r, float rf, float h)
{
  float sdf = sdfStar2d(p.xy, r, rf);
  vec2 w = vec2(sdf, abs(p.z) - h);
  return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
}

float opSmoothUnion(float d1, float d2, float k)
{
  float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
  return mix(d2, d1, h) - k * h * (1.0 - h);
}


mat3 rotateZ = mat3(cos(Params.time * 0.4), -sin(Params.time * 0.4), 0,
                    sin(Params.time * 0.4), cos(Params.time * 0.4), 0,
                    0, 0, 1);

mat3 rotateY = mat3(cos(Params.time), 0, -sin(Params.time),
                    0, 1, 0,
                    sin(Params.time), 0, cos(Params.time));

float sdfFog(vec3 p)
{
  float hillsFog = sdfBox(p - vec3(0, Params.hillsInfo.y, 0), Params.hillsInfo.xwz);

  // float tree_base = sdfCylinder(p - vec3(0, 4, 0), 1.5, .75);
  float tree = sdfCone(p - vec3(0, 18 + Params.hillsInfo.y + Params.hillsInfo.w, 0), tan(3.14159 / 8.0), 8.0) - .1;
  tree = opSmoothUnion(tree, sdfCone(p - vec3(0, 16 + Params.hillsInfo.y + Params.hillsInfo.w, 0), tan(3.14159 / 8.0), 10.0) - .1, 1.0);
  tree = opSmoothUnion(tree, sdfCone(p - vec3(0, 14 + Params.hillsInfo.y + Params.hillsInfo.w, 0), tan(3.14159 / 8.0), 14.0) - .1, 1.0);
  mat3 rotate36 = mat3(cos(PI * 0.2), -sin(PI * 0.2), 0,
                      sin(PI * 0.2), cos(PI * 0.2), 0,
                      0, 0, 1);
  float star = sdfStar(rotate36 * rotateY * (p - vec3(0, 18 + Params.hillsInfo.y + Params.hillsInfo.w, 0)), 1.5, 2.0, 1.0);
  return min(min(tree, hillsFog), star);
}

float BeerLambert(float absorbtionCoefficient, float dist)
{
  return exp(-absorbtionCoefficient * dist);
}

float hash(float n)
{
  return fract(sin(n) * 1000.0);
}

float noise(vec3 p, float s, float t, float v)
{
  vec3 i = floor(p);
  vec3 frac = fract(p);

  vec3 terp = frac * frac * (3.0 - 2.0 * frac);

  float n = dot(i, vec3(s, t, v));

  return mix(
          mix(
            mix(hash(n),     hash(n + s),     terp.x),
            mix(hash(n + t), hash(n + s + t), terp.x),
          terp.y),
          mix(
            mix(hash(n + v),     hash(n + s + v),     terp.x),
            mix(hash(n + t + v), hash(n + s + t + v), terp.x),
          terp.y), 
        terp.z);
}

float fbm(vec3 p, float s, float t, float v)
{
  float ret = 0.0;
  float amp = 0.4;
  for (int i = 0; i < 3; ++i) {
    ret += amp * noise(p, s, t, v);
    p *= 3.0;
    amp *= 0.7;
  }

  return ret;
}

float getDensity(vec3 p, float sdf)
{
  return min(fbm(rotateZ * rotateY * p, 103.0, 21.0, 46.0), -sdf);
}

void main()
{
  const int MAX_STEPS = 1000;
  const float STEP_SIZE = 0.1;
  const float LIGHT_STEP_SIZE = 1.0;
  const float LIGHT_R = 20.0;
  const float LIGHT_INNER_R = 10.0;
  const vec3 FOG_CLEAR_COLOR = vec3(fbm(vec3(-2.1, 2.0, 1.0) * Params.time * 0.05, 10.0, 12.0, 42.0),
                                    fbm(vec3(0.3, 1.2, 2.3) * Params.time * 0.05, 23.0, 48.0, 92.0),
                                    fbm(vec3(-1.3, 2.0, -2.3) * Params.time * 0.05, 42.0, 39.0, 27.0));
  const vec3 AMBIENT_COLOR = vec3(0.2);

  float opacity = 1.0;
  vec3 accumulation = vec3(0.0);

  vec3 viewVec = getViewDir() * STEP_SIZE;
  vec3 curPos = Params.camPos.xyz;
  float distToSurface = getDepth();

  for (int i = 0; i < MAX_STEPS; ++i) 
  {
    if (distToSurface < 0.0 || opacity < 0.01)
      break;

    float sdf = sdfFog(curPos);
    if (sdf < 0.0) 
    {
      float prevOpacity = opacity;
      opacity *= BeerLambert(0.7 * getDensity(curPos, sdf), STEP_SIZE);
      accumulation += (prevOpacity - opacity) * FOG_CLEAR_COLOR * AMBIENT_COLOR;

      float distToLight = length(Params.lightPos - curPos);
      vec3 toLightVec = normalize(Params.lightPos - curPos) * LIGHT_STEP_SIZE;
      vec3 toLightPos = curPos;
      float toLightOpacity = 1.0;
      float atten = clamp((LIGHT_R - distToLight + LIGHT_INNER_R) / (LIGHT_R - LIGHT_INNER_R), 0.0, 1.0);
      for (int j = 0; j < 100; ++j)
      {
        if (distToLight < 0.0 || toLightOpacity < 0.01)
          break;
        
        float sdf = sdfFog(toLightPos);
        if (sdf < 0.0)
          toLightOpacity *= BeerLambert(0.7 * getDensity(toLightPos, sdf), LIGHT_STEP_SIZE);

        distToLight -= LIGHT_STEP_SIZE;
        toLightPos += toLightVec;
      }

      accumulation += (prevOpacity - opacity) * vec3(1.0, 0.9, 0.6) * FOG_CLEAR_COLOR * toLightOpacity * atten;
    }

    distToSurface -= STEP_SIZE;
    curPos += viewVec;
  }

  color = vec4(accumulation, opacity);
}
