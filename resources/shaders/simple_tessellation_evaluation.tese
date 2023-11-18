#version 450

layout(quads, equal_spacing, ccw) in;

layout(location = 0) in TCS_OUT
{
  vec2 texCoord;
} tcOut[];

layout(location = 0) patch out TES_OUT
{
	vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} teOut;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  vec3 scaleAndOffset;
  float minHeight;
  float maxHeight;
  float tes_level;
} params;

layout(binding = 0, set = 0) uniform sampler2D heightMap;

void main()
{
	vec2 tex1 = mix(tcOut[0].texCoord, tcOut[1].texCoord, gl_TessCoord.x);
	vec2 tex2 = mix(tcOut[2].texCoord, tcOut[3].texCoord, gl_TessCoord.x);

	teOut.texCoord = mix(tex1, tex2, gl_TessCoord.y);

	vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);

	teOut.wPos = mix(p1, p2, gl_TessCoord.y).xyz;
	float h = texture(heightMap, teOut.texCoord).r;
	teOut.wPos.y += mix(params.minHeight, params.maxHeight, h);

	float scale = 100.0;
	float h1 = texture(heightMap, teOut.texCoord + vec2(1, 0) / 2048).r;
	float h2 = texture(heightMap, teOut.texCoord + vec2(0, 1) / 2048).r;
	teOut.wNorm = normalize(vec3((h1 - h) * scale, 1, (h2 - h) * scale));

	gl_Position = params.mProjView * vec4(teOut.wPos, 1.0);
}