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
  vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform sampler2D heightMap;

void main(void)
{
  //// unoptimized verion
  // {
  //   const float offset = 1.0 / params.tes_level;
  //   int vertexID = gl_VertexIndex % 3 << gl_VertexIndex / 3;
  //   vOut.texCoord = vec2(int((vertexID & 3) != 0) + gl_InstanceIndex % params.tes_level, int(vertexID > 1) + gl_InstanceIndex / params.tes_level) * offset;

  //   vec3 v0 = (vec3(vOut.texCoord.x - offset, 1.0, vOut.texCoord.y - offset) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vec3 v1 = (vec3(vOut.texCoord.x, 1.0, vOut.texCoord.y - offset) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vec3 v2 = (vec3(vOut.texCoord.x - offset, 1.0, vOut.texCoord.y) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vOut.wPos = (vec3(vOut.texCoord.x, 1.0, vOut.texCoord.y) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vec3 v3 = (vec3(vOut.texCoord.x + offset, 1.0, vOut.texCoord.y) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vec3 v4 = (vec3(vOut.texCoord.x, 1.0, vOut.texCoord.y + offset) * 2.0 - 1.0) * params.scaleAndOffset;
  //   vec3 v5 = (vec3(vOut.texCoord.x + offset, 1.0, vOut.texCoord.y + offset) * 2.0 - 1.0) * params.scaleAndOffset;

  //   v0.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord - vec2(offset)).r);
  //   v1.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord - vec2(0, offset)).r);
  //   v2.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord - vec2(offset, 0)).r);
  //   vOut.wPos.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord).r);
  //   v3.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord + vec2(offset, 0)).r);
  //   v4.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord + vec2(0, offset)).r);
  //   v5.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord + vec2(offset, offset)).r);

  //   v0 -= vOut.wPos;
  //   v1 -= vOut.wPos;
  //   v2 -= vOut.wPos;
  //   v3 -= vOut.wPos;
  //   v4 -= vOut.wPos;
  //   v5 -= vOut.wPos;

  //   vOut.wNorm = normalize(cross(v0, v2) + cross(v2, v4) + cross(v4, v5) + cross(v5, v3) + cross(v3, v1) + cross(v1, v0));
  // }

  {
    float offset = 1.0 / params.tes_level;
    int vertexID = gl_VertexIndex % 3 << gl_VertexIndex / 3;
    vOut.texCoord = vec2(int((vertexID & 3) != 0) + gl_InstanceIndex % params.tes_level, int(vertexID > 1) + gl_InstanceIndex / params.tes_level) * offset;
    offset = offset;
    float h_c = texture(heightMap, vOut.texCoord).r;
    vOut.wPos = (vec3(vOut.texCoord.x, 1.0, vOut.texCoord.y) * 2.0 - 1.0) * params.scaleAndOffset;
    vOut.wPos.y += mix(params.minHeight, params.maxHeight, h_c);

    float h0 = texture(heightMap, vOut.texCoord - vec2(offset)).r;
    float h1 = texture(heightMap, vOut.texCoord - vec2(0, offset)).r;
    float h2 = texture(heightMap, vOut.texCoord - vec2(offset, 0)).r;
    float h3 = texture(heightMap, vOut.texCoord + vec2(offset, 0)).r;
    float h4 = texture(heightMap, vOut.texCoord + vec2(0, offset)).r;
    float h5 = texture(heightMap, vOut.texCoord + vec2(offset, offset)).r;

    offset *= 2.0;

    vec3 v0 = vec3(-offset * params.scaleAndOffset.x, (params.maxHeight - params.minHeight) * (h0 - h_c), -offset * params.scaleAndOffset.z);
    vec3 v1 = vec3(                                0, (params.maxHeight - params.minHeight) * (h1 - h_c), -offset * params.scaleAndOffset.z);
    vec3 v2 = vec3(-offset * params.scaleAndOffset.x, (params.maxHeight - params.minHeight) * (h2 - h_c),                                 0);
    vec3 v3 = vec3( offset * params.scaleAndOffset.x, (params.maxHeight - params.minHeight) * (h3 - h_c),                                 0);
    vec3 v4 = vec3(                                0, (params.maxHeight - params.minHeight) * (h4 - h_c),  offset * params.scaleAndOffset.z);
    vec3 v5 = vec3( offset * params.scaleAndOffset.x, (params.maxHeight - params.minHeight) * (h5 - h_c),  offset * params.scaleAndOffset.z);

    vOut.wNorm = normalize(cross(v0 - v4, v2) + cross(v4 - v3, v5) + cross(v3 - v0, v1));
  }

  gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
}
