#version 450

layout(location = 0) out vec4 shadowMap;

// for compatibility
layout(location = 0) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

void main()
{
  float z = gl_FragCoord.z;
  shadowMap.xy = vec2(z, z * z);
}