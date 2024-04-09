#version 460

#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(quads, fractional_odd_spacing, ccw) in;

layout(location = 0) in vec2 texCoord_in[];
layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 wNorm;
layout(location = 2) out vec3 wPos;

layout(push_constant) uniform push_const
{
    mat4 mProjViewWorld;
};

layout(binding = 0) uniform sampler2D heightMap;

layout(binding = 1) uniform Data
{
    TessellationParams params; 
};

#define GET_F(i, j) (vec3(gl_Position.x + (i) * pos_offset.x, \
    params.quadHeight * textureLod(heightMap, texCoord + vec2((i), (j)) * uv_offset, 0).r, \
    gl_Position.z + (j) * pos_offset.z))

void main()
{
    vec2 uv_down = mix(texCoord_in[0], texCoord_in[1], gl_TessCoord.x);
    vec2 uv_up = mix(texCoord_in[2], texCoord_in[3], gl_TessCoord.x);

    vec4 pos_down = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 pos_up = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);

    texCoord = mix(uv_down, uv_up, gl_TessCoord.y);
    
    gl_Position = mix(pos_down, pos_up, gl_TessCoord.y);
    gl_Position.y = params.quadHeight * textureLod(heightMap, texCoord, 0).r;

    vec2 uv_offset = vec2((texCoord_in[1].x - texCoord_in[0].x) / gl_TessLevelInner[0], (texCoord_in[2].y - texCoord_in[0].y) / gl_TessLevelInner[1]);
    vec4 pos_offset = (gl_in[1].gl_Position - gl_in[0].gl_Position) / gl_TessLevelInner[0] + (gl_in[2].gl_Position - gl_in[0].gl_Position) / gl_TessLevelInner[1];
    
    vec3 pos0 = GET_F(-1, -1) - gl_Position.xyz;
    vec3 pos1 = GET_F( 0, -1) - gl_Position.xyz;
    vec3 pos2 = GET_F( 1, -1) - gl_Position.xyz;
    vec3 pos7 = GET_F(-1,  0) - gl_Position.xyz;
    vec3 pos3 = GET_F( 1,  0) - gl_Position.xyz;
    vec3 pos6 = GET_F(-1,  1) - gl_Position.xyz;
    vec3 pos5 = GET_F( 0,  1) - gl_Position.xyz;
    vec3 pos4 = GET_F( 1,  1) - gl_Position.xyz;

    wNorm = cross(pos1, pos0) + cross(pos2, pos1);
    wNorm += cross(pos3, pos2) + cross(pos4, pos3);
    wNorm += cross(pos5, pos4) + cross(pos6, pos5);
    wNorm += cross(pos7, pos6) + cross(pos0, pos7);
    wNorm = normalize(wNorm);

    wPos = gl_Position.xyz; 
    gl_Position = mProjViewWorld * gl_Position;
}