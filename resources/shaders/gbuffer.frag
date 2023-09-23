#version 450

layout(std430, push_constant) uniform params_t
{
  mat4 mProjView;
  uint meshID;
} params;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normal;

layout(set = 1, binding = 0) uniform sampler2D bunnyAlbedo;
layout(set = 1, binding = 1) uniform sampler2D bunnyNormal;
layout(set = 1, binding = 2) uniform sampler2D bunnyRoughness;

layout(set = 2, binding = 0) uniform sampler2D teapotAlbedo;
layout(set = 2, binding = 1) uniform sampler2D teapotNormal;
layout(set = 2, binding = 2) uniform sampler2D teapotMetalness;
layout(set = 2, binding = 3) uniform sampler2D teapotRoughness;

layout(set = 3, binding = 0) uniform sampler2D flatAlbedo;
layout(set = 3, binding = 1) uniform sampler2D flatNormal;
layout(set = 3, binding = 2) uniform sampler2D flatRoughness;

layout (location = 0) in FS_IN
{
  vec3 wPos;
  vec3 wTangent;
  vec3 wBitangent;
  vec3 wNorm;
  vec2 texCoord;
} fsIn;

void main() {
  vec3 wT = fsIn.wTangent;
  vec3 wB = fsIn.wBitangent;
  vec3 wN = fsIn.wNorm;

  vec2 tex = fsIn.texCoord;

  vec3 c;
  vec3 n;
  float metalness;
  float roughness;
  switch (params.meshID) {
  case 0:
    c = texture(bunnyAlbedo, tex).rgb;
    n = texture(bunnyNormal, tex).rgb * 2.0 - 1.0;
    metalness = 0.0;
    roughness = texture(bunnyRoughness, tex).r;
    break;
  case 1:
    c = texture(teapotAlbedo, tex).rgb;
    n = texture(teapotNormal, tex).rgb * 2.0 - 1.0;
    metalness = texture(teapotMetalness, tex).r;
    roughness = texture(teapotRoughness, tex).r;
    break;    
  case 2:
    c = texture(flatAlbedo, 16 * tex).rgb;
    n = texture(flatNormal, 16 * tex).rgb * 2.0 - 1.0;
    metalness = 0.0;
    roughness = texture(flatRoughness, 16 * tex).r;
    break;
  }

  color = vec4(c, metalness);
  normal = vec4(normalize(n.r * wT - n.g * wB + n.b * wN), roughness);
}
