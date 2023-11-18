#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 scaleAndOffset;
  float minHeight;
  float maxHeight;
  int tes_level;
} params;

layout(location = 0) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main(void)
{
  if(gl_VertexIndex == 0)
  {
    gl_Position = vec4(-params.scaleAndOffset.x, params.scaleAndOffset.y, -params.scaleAndOffset.z, 1.0);
    vOut.texCoord = vec2(0.0f, 0.0f);
  }
  else if(gl_VertexIndex == 1) 
  {
    gl_Position = vec4(params.scaleAndOffset.x, params.scaleAndOffset.y, -params.scaleAndOffset.z, 1.0);
    vOut.texCoord = vec2(1.0f, 0.0f);
  }
  else if(gl_VertexIndex == 2) 
  {
    gl_Position = vec4(-params.scaleAndOffset.x, params.scaleAndOffset.y, params.scaleAndOffset.z, 1.0);
    vOut.texCoord = vec2(0.0f, 1.0f);
  }
  else
  { 
    gl_Position = vec4(params.scaleAndOffset.x, params.scaleAndOffset.y, params.scaleAndOffset.z, 1.0);
    vOut.texCoord = vec2(1.0f, 1.0f);
  }
}
