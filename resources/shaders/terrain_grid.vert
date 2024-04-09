#version 460

#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec2 texCoord;

layout(binding = 1) uniform Data
{
    TessellationParams params; 
};

void main()
{
    texCoord = vec2((gl_VertexIndex & 1) + gl_InstanceIndex % params.sqrtPatchCount, int(gl_VertexIndex > 1) + gl_InstanceIndex / params.sqrtPatchCount) / params.sqrtPatchCount;
    gl_Position = vec4((texCoord.x - 0.5) * params.quadHalfLength, 0, (texCoord.y - 0.5) * params.quadHalfLength, 1.0);
}