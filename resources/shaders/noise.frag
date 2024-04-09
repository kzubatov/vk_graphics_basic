#version 450

layout(push_constant) uniform params_t
{
  int red_y_scale;
  float red_noise_scale;
  int width;
  int height;
} params;

layout(location = 0) in VS_OUT 
{
	vec2 texCoord;
} vOut;

layout(location = 0) out vec4 color;

// get random vector
vec2 grad(ivec2 z, int s)
{
  // 2D to 1D
  int n = z.x + z.y * s;

  // Hugo Elias hash
  n = (n << 13) ^ n;
  n = (n * (n * n * 15731 + 789221) + 1376312589) >> 16;

  // Perlin style vectors
  n &= 7;
  vec2 gr = vec2(n & 1,n >> 1) * 2.0 - 1.0;
  return n >= 6 ? vec2(0.0, gr.x) : n >= 4 ? vec2(gr.x, 0.0) : gr;     
}

float noise(vec2 p, int s)
{
  ivec2 i = ivec2(floor(p));
  vec2 f = fract(p);
	
  // smoothstep
	vec2 u = f * f * (3.0 - 2.0 * f);

  return mix(mix(dot(grad(i, s), f), dot(grad(i + ivec2(1, 0), s), f - vec2(1.0, 0.0)), u.x),
              mix(dot(grad(i + ivec2(0, 1), s), f - vec2(0.0, 1.0)), 
                  dot(grad(i + ivec2(1, 1), s), f - vec2(1.0, 1.0)), u.x), u.y);
}

float getHeight(vec2 uv)
{
	float f;
  mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
	
  f = 0.5 * noise(uv, params.red_y_scale); uv = m * uv;
  f += 0.25 * noise(uv, params.red_y_scale); uv = m * uv;
  f += 0.125 * noise(uv, params.red_y_scale); uv = m * uv;
  f += 0.0625 * noise(uv, params.red_y_scale); uv = m * uv;
  f += 0.03125 * noise(uv, params.red_y_scale);

	return 0.5 * f + 0.5;
}

void main()
{
	vec2 uv = vOut.texCoord;
	vec2 offset = 0.5 / vec2(params.width, params.height);

  float height = 0.25 * (getHeight((uv - offset) * params.red_noise_scale)
    + getHeight((uv + vec2(offset.x, -offset.y)) * params.red_noise_scale)
    + getHeight((uv + vec2(-offset.x, offset.y)) * params.red_noise_scale)
    + getHeight((uv + offset) * params.red_noise_scale));
  
  uv += vec2(2.0 * offset.x, 0);

  float height_dx = 0.25 * (getHeight((uv - offset) * params.red_noise_scale)
    + getHeight((uv + vec2(offset.x, -offset.y)) * params.red_noise_scale)
    + getHeight((uv + vec2(-offset.x, offset.y)) * params.red_noise_scale)
    + getHeight((uv + offset) * params.red_noise_scale));

  uv = vOut.texCoord + vec2(0, 2.0 * offset.y);

  float height_dy = 0.25 * (getHeight((uv - offset) * params.red_noise_scale)
    + getHeight((uv + vec2(offset.x, -offset.y)) * params.red_noise_scale)
    + getHeight((uv + vec2(-offset.x, offset.y)) * params.red_noise_scale)
    + getHeight((uv + offset) * params.red_noise_scale));

  color = vec4(normalize(vec3((height_dx - height) * 0.5 / offset.x, 1, (height_dy - height) * 0.5 / offset.y)) * 0.5 + 0.5, height);
}