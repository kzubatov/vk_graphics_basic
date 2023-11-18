#version 450

layout(push_constant, std430) uniform params_t
{
  float seed;
  float scale;
} params;

layout(location = 0) in VS_OUT 
{
	vec2 texCoord;
} vOut;

layout(location = 0) out vec4 color;

float hash(vec2 a)
{
  return fract(sin(a.x * 3433.8 + a.y * 3843.98) * 45933.8) * (params.seed + 0.2);
}

float noise(vec2 p)
{
  vec2 id = floor(p);
  p = fract(p);
  p = smoothstep(0.0, 1.0, p);  

  vec2 A = vec2(hash(id), hash(id + vec2(0,1)));
  vec2 B = vec2(hash(id + vec2(1,0)), hash(id + vec2(1,1)));
  vec2 C = mix(A, B, p.x);

  return mix(C.x, C.y, p.y);
}

void main() 
{
  color.r = noise(vOut.texCoord * params.scale);
}