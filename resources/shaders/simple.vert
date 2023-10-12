#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    uint offset;
} params;

layout(set = 1, binding = 0) readonly buffer ModelMatrix
{
    mat4 modelMatrix[];
};

layout(set = 1, binding = 1) readonly buffer visible
{
    uint visibleInstances[];
};

layout(location = 0) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
} vOut;

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);

    mat4 mModel = modelMatrix[params.offset + visibleInstances[params.offset + gl_InstanceIndex]];
    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
