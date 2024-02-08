#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mPrev;
    mat4 mCur;
} params;

layout(location = 0) out VS_OUT
{
    vec4 prevPos;
} vOut;

out gl_PerVertex { vec4 gl_Position; };

void main(void)
{
    vOut.prevPos = params.mPrev * vec4(vPosNorm.xyz, 1.0f);
    gl_Position = params.mCur * vec4(vPosNorm.xyz, 1.0f);
}
