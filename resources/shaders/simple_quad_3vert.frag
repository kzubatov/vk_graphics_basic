#version 450

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D colorMap;

layout(location = 0) in vec2 texCoords;

void main()
{
  color = textureLod(colorMap, texCoords, 0);
}