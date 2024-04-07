#version 450

layout(vertices = 4) out;

layout(location = 0) in vec2 texCoord[];

layout(location = 0) out TCS_OUT
{
  vec2 texCoord;
} tcOut[];

// layout(push_constant) uniform params_t
// {
//   mat4 mProjView;
//   vec3 scaleAndOffset;
//   float minHeight;
//   float maxHeight;
//   int level;
// } params;

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  tcOut[gl_InvocationID].texCoord = texCoord[gl_InvocationID];

  gl_TessLevelInner[0] = 2;
  gl_TessLevelInner[1] = 2;
  gl_TessLevelOuter[0] = 2;
  gl_TessLevelOuter[1] = 2;
  gl_TessLevelOuter[2] = 2;
  gl_TessLevelOuter[3] = 4;
}