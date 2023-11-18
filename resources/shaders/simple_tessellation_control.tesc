#version 450

layout(vertices = 4) out;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} vOut[];

layout(location = 0) out TCS_OUT
{
  vec2 texCoord;
} tcOut[];

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 scaleAndOffset;
  float minHeight;
  float maxHeight;
  float level;
} params;

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  
  tcOut[gl_InvocationID].texCoord = vOut[gl_InvocationID].texCoord;
  
  if (gl_InvocationID == 0)
  {
    gl_TessLevelInner[0] = params.level;
    gl_TessLevelInner[1] = params.level;
    gl_TessLevelOuter[0] = params.level;
    gl_TessLevelOuter[1] = params.level;
    gl_TessLevelOuter[2] = params.level;
    gl_TessLevelOuter[3] = params.level;
  }
}