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

// get random vector
vec2 grad(ivec2 z) 
{
  // 2D to 1D
  int n = z.x + z.y * 11111;

  // Hugo Elias hash
  n = (n << 13) ^ n;
  n = (n * (n * n * 15731 + 789221) + 1376312589) >> 16;

  // Perlin style vectors
  n &= 7;
  vec2 gr = vec2(n & 1,n >> 1) * 2.0 - 1.0;
  return n >= 6 ? vec2(0.0, gr.x) : n >= 4 ? vec2(gr.x, 0.0) : gr;     
}

float noise(vec2 p)
{
  ivec2 i = ivec2(floor(p));
  vec2 f = fract(p);
	
  // smoothstep
	vec2 u = f * f * (3.0 - 2.0 * f);

  return mix(mix(dot(grad(i), f), dot(grad(i + ivec2(1, 0)), f - vec2(1.0, 0.0)), u.x),
              mix(dot(grad(i + ivec2(0, 1)), f - vec2(0.0, 1.0)), 
                  dot(grad(i + ivec2(1, 1)), f - vec2(1.0, 1.0)), u.x), u.y);
}

void main()
{
	vec2 uv = vOut.texCoord * params.scale + params.seed * 4;
	
	float f = 0.0;
  mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
	
  f = 0.5 * noise(uv); uv = m * uv;
  f += 0.25 * noise(uv); uv = m * uv;
  f += 0.125 * noise(uv); uv = m * uv;
  f += 0.0625 * noise(uv); uv = m * uv;
	color.r = 0.5 + 0.5 * f;
}