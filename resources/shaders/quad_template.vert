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
  int vertexID = gl_VertexIndex % 3 << gl_VertexIndex / 3;
  vOut.texCoord = vec2(int((vertexID & 3) != 0)  + gl_InstanceIndex % params.tes_level, int(vertexID > 1) + gl_InstanceIndex / params.tes_level) / params.tes_level;

  vOut.wPos = vec3(vOut.texCoord.x * 2. - 1., 1, vOut.texCoord.y * 2. - 1.) * params.scaleAndOffset;
  vOut.wPos.y += mix(params.minHeight, params.maxHeight, texture(heightMap, vOut.texCoord).r);

  float eps = 0.01;
	float h1 = texture(heightMap, vOut.texCoord + vec2(eps, 0)).r;
	float h2 = texture(heightMap, vOut.texCoord - vec2(eps, 0)).r;
	float h3 = texture(heightMap, vOut.texCoord + vec2(0, eps)).r;
	float h4 = texture(heightMap, vOut.texCoord - vec2(0, eps)).r;

	vOut.wNorm = normalize(vec3((h2 - h1) / (2 * eps), 1, (h4 - h3) / (2 * eps)));

  gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
}
