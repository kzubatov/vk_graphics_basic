#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main() {
  vec2 xy = vec2((gl_VertexIndex & 1) << 2, (gl_VertexIndex & 2) << 1) - 1;
  gl_Position  = vec4(xy, 0, 1);
  vOut.texCoord = xy * 0.5 + 0.5;
}