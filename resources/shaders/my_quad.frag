#version 450

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D colorTex;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  color = textureLod(colorTex, surf.texCoord, 0);
}
