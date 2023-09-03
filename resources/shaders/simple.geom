#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.h"

layout (triangles) in;
layout (triangle_strip, max_vertices = 12) out;

layout(set = 0, binding = 0) uniform AppData {
  UniformParams Params;
};

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
} params;

layout (location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
  uint instanceID;
} vOut[];

layout (location = 0) out GS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} gOut;

void main(void) {
  float u = abs(sin(Params.time));
  float v = (1.0f - u) * abs(sin(Params.time));
  float w = 1.0f - u - v;

  vec3 fourth_wNorm = vOut[0].wNorm * u + vOut[1].wNorm * v + vOut[2].wNorm * w;
  
  vec3 fourth_wPos = vOut[0].wPos * u + vOut[1].wPos * v + vOut[2].wPos * w;
  fourth_wPos += max(sin(Params.time * 0.4), 0.05) * 0.06 * fourth_wNorm; 

  vec3 fourth_wTangent = vOut[0].wTangent * u + vOut[1].wTangent * v + vOut[2].wTangent * w;
  vec2 fourth_texCoord = vOut[0].texCoord * u + vOut[1].texCoord * v + vOut[2].texCoord * w;

  for (uint i = 0; i < 3; ++i) {
    gOut.wPos = vOut[i].wPos;
    gOut.wNorm = vOut[i].wNorm;
    gOut.wTangent = vOut[i].wTangent;
    gOut.texCoord = vOut[i].texCoord;

    gl_Position = params.mProjView * vec4(vOut[i].wPos, 1.0);
    EmitVertex();
  }
  EndPrimitive();

  bool instance = vOut[0].instanceID == 1 || vOut[0].instanceID == 5;
  for (uint i = 0; instance && i < 3; ++i) {
    gOut.wPos = vOut[i].wPos;
    gOut.wNorm = vOut[i].wNorm;
    gOut.wTangent = vOut[i].wTangent;
    gOut.texCoord = vOut[i].texCoord;

    gl_Position = params.mProjView * vec4(vOut[i].wPos, 1.0);
    EmitVertex();

    gOut.wPos = vOut[(i + 1) % 3].wPos;
    gOut.wNorm = vOut[(i + 1) % 3].wNorm;
    gOut.wTangent = vOut[(i + 1) % 3].wTangent;
    gOut.texCoord = vOut[(i + 1) % 3].texCoord;

    gl_Position = params.mProjView * vec4(vOut[(i + 1) % 3].wPos, 1.0);
    EmitVertex();

    gOut.wPos = fourth_wPos;
    gOut.wNorm = fourth_wNorm;
    gOut.wTangent = fourth_wTangent;
    gOut.texCoord = fourth_texCoord;

    gl_Position = params.mProjView * vec4(fourth_wPos, 1.0);
    EmitVertex();
    EndPrimitive();
  }
}