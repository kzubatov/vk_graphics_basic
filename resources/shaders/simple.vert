#version 450
#define M_PI 3.1415926535897932384626433832795
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    float time;
} params;

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

mat4 rotateX(float time) {
    return mat4(1, 0, 0, 0,
                0, cos(time), sin(time), 0,
                0, -sin(time), cos(time), 0,
                0, 0, 0, 1);
}

mat4 rotateY(float time) {
    return mat4(cos(time), 0, -sin(time), 0,
                0, 1, 0, 0,
                sin(time), 0, cos(time), 0,
                0, 0, 0, 1);
}

mat4 rotateZ(float time) {
    return mat4(cos(time), sin(time), 0, 0,
                -sin(time), cos(time), 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1);
}

mat4 transfer(vec3 t) {
    return mat4(1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                t.x, t.y, t.z, 1);
}

mat4 scale(vec3 s) {
    return mat4(s.x, 0, 0, 0,
                0, s.y, 0, 0,
                0, 0, s.z, 0,
                0, 0, 0, 1);
}


out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    mat4 mModel;
    float part = fract(params.time);
    switch (gl_InstanceIndex) {
    case 0:
        mModel = transfer(vec3(0, 0, -0.5)) * scale(vec3(1.0 + (1.0 - cos(params.time)) / 4.0, 1, 1.6)) * params.mModel;
        break;
    case 1:
        mModel = params.mModel * scale(vec3(1.0, mix(1.0, 1.5, (cos(params.time + M_PI / 2.0) + 1.0) / 2.0), 1.0));
        break;
    case 2:
        mModel = transfer(vec3((1.0 - cos(params.time)) / 4.0, 0, 0)) * params.mModel;
        break;
    case 4:
        mModel = params.mModel * rotateY(mix(0.0, M_PI / 4.0, (-cos(params.time) + 1) / 2.0)) * transfer(vec3(-0.575, 0, 0)); 
        break;
    case 5:
        mModel = transfer(vec3(sin(4.0 * params.time) * cos(params.time), sin(4.0 * params.time) * sin(params.time) - 0.5, -0.3)) * params.mModel * rotateY(params.time) * scale(vec3(0.25));
        break;
    default:
        mModel = params.mModel;
    }

    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
