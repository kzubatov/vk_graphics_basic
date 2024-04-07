#version 450

// layout(push_constant) uniform params_t
// {
//   mat4 mProjView;
//   int quads_per_length;
//   int field_length;
// };

layout(location = 0) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main(void)
{
  // vOut.texCoord = vec2((gl_VertexIndex & 1) + gl_InstanceIndex % quads_per_length, (gl_VertexIndex >> 1) + gl_InstanceIndex / quads_per_length) / quads_per_length;
  // gl_Position = vec4((vec3(vOut.texCoord.x, .5, vOut.texCoord.y) - 0.5) * field_length, 1.0);
  vOut.texCoord = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
  gl_Position = vec4(vOut.texCoord.x * 100, 0.0, vOut.texCoord.y * 100, 1.0);
}
