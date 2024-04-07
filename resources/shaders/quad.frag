#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;
layout(location = 0) in vec2 texCoord;
// layout(location = 1) noperspective in vec3 d;

// layout (binding = 0) uniform sampler2D colorTex;

// layout (location = 0 ) in VS_OUT
// {
//   vec2 texCoord;
// } surf;

void main()
{
  // color = textureLod(colorTex, surf.texCoord, 0);
  color = vec4(texCoord,0,1);
  // float min_d = min(d[0], min(d[1], d[2]));
  // color.rgb = mix(vec3(1,1,1), color.rgb, smoothstep(0.0, 1.0, 20 * min_d));
}
