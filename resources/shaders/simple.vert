#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(location = 0) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
#ifdef TAA_PASS_DYNAMIC
    vec4 clipPosPrev;
    vec4 clipPosCur;
#endif
} vOut;

#ifdef TAA_PASS_DYNAMIC
layout(binding = 2) uniform taa_info_t {
    mat4 mProjViewWorldPrev;
    vec2 offset;
} taa_info;
#elif defined(TAA_PASS_STATIC)
layout(binding = 2) uniform taa_info_t {
    vec2 offset;
} taa_info;
#endif

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
#ifdef TAA_PASS_DYNAMIC
    vOut.clipPosCur = gl_Position;
    vOut.clipPosPrev = taa_info.mProjViewWorldPrev * vec4(vPosNorm.xyz, 1.0);
    gl_Position.xy += taa_info.offset * gl_Position.w;
#elif defined(TAA_PASS_STATIC)
    gl_Position.xy += taa_info.offset * gl_Position.w;
#endif
}
