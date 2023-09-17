#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(std430, push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 cameraPos;
  uint meshID;
} params;


layout (location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wTangent;
  vec3 wBitangent;
  vec3 wNorm;
  vec2 texCoord;
} vOut;

layout (std430, set = 0, binding = 0) readonly buffer matrices {
  mat4 modelMatrix[];
};

layout (std430, set = 0, binding = 1) readonly buffer visible {
  uint visibleInstances[];
};

struct meshInfo {
  vec4 center;
  float r;
  uint instanceCount;
};

layout (std430, set = 0, binding = 2) readonly buffer mesh_info {
  meshInfo mInfo[];
};

out gl_PerVertex { vec4 gl_Position; };

void main(void)
{
  uint offset = 0u;
  for (uint i = 0; i < params.meshID; ++i) {
    offset += mInfo[i].instanceCount; 
  }

  const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
  const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

  vOut.wPos       = (modelMatrix[offset + visibleInstances[offset + gl_InstanceIndex]] * vec4(vPosNorm.xyz, 1.0f)).xyz;
  vOut.wNorm      = normalize(mat3(transpose(inverse(modelMatrix[offset + visibleInstances[offset + gl_InstanceIndex]]))) * wNorm.xyz);
  vOut.wTangent   = normalize(mat3(transpose(inverse(modelMatrix[offset + visibleInstances[offset + gl_InstanceIndex]]))) * wTang.xyz);
  vOut.wBitangent = cross(vOut.wNorm, vOut.wTangent);
  vOut.texCoord   = vTexCoordAndTang.xy;

  gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
