#version 460

#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(vertices = 4) out;

layout(location = 0) in vec2 texCoord_in[];
layout(location = 0) out vec2 texCoord_out[];

layout(binding = 0) uniform sampler2D heightMap;

layout(binding = 1) uniform Data
{
    TessellationParams params; 
};

float dLodSphere(vec4 p0, vec4 p1, vec2 t0, vec2 t1)
{
	p0.y = params.quadHeight * textureLod(heightMap, t0, 0).a;
	p1.y = params.quadHeight * textureLod(heightMap, t1, 0).a;

	vec4 center = 0.5 * (p0 + p1);
	vec4 view0 = params.mViewWorld * center;
	vec4 view1 = view0;
	view1.x += distance(p0, p1);

	vec4 clip0 = params.mProj * view0;
	vec4 clip1 = params.mProj * view1;

	clip0 /= clip0.w;
	clip1 /= clip1.w;

	vec2 screen0 = (clip0.xy + 1.0) * 0.5 * params.resolution;
	vec2 screen1 = (clip1.xy + 1.0) * 0.5 * params.resolution;
	float d = distance(screen0, screen1);

	return clamp(d / params.triangleSize, float(params.tessMinLevel), float(params.tessMaxLevel));
}

void main()
{
    gl_TessLevelOuter[0] = dLodSphere(gl_in[0].gl_Position, gl_in[2].gl_Position, texCoord_in[0], texCoord_in[2]);
    gl_TessLevelOuter[1] = dLodSphere(gl_in[0].gl_Position, gl_in[1].gl_Position, texCoord_in[0], texCoord_in[1]);
    gl_TessLevelOuter[2] = dLodSphere(gl_in[1].gl_Position, gl_in[3].gl_Position, texCoord_in[1], texCoord_in[3]);
    gl_TessLevelOuter[3] = dLodSphere(gl_in[2].gl_Position, gl_in[3].gl_Position, texCoord_in[2], texCoord_in[3]);

    gl_TessLevelInner[0] = (gl_TessLevelOuter[1] + gl_TessLevelOuter[3]) * 0.5;
    gl_TessLevelInner[1] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[2]) * 0.5;

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    texCoord_out[gl_InvocationID] = texCoord_in[gl_InvocationID];
}