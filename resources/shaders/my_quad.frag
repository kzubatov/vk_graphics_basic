#version 450
#extension GL_ARB_separate_shader_objects : enable

// getSampleExperimental doesn't work!! work in prog
// #define MEDIAN
#define BILATERAL
const vec3 h = vec3(0.5); // bilateral filter: exp(-(i^2 + j^2) / size^2 - colorDif^2 / h^2)

layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

const int distToBorder = 1;
const int size = 2 * distToBorder + 1;

void getSample(out vec3 c[size * size]) {
  vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));

  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      c[j + distToBorder + (distToBorder + i) * size] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
    }
  }
}

void getSampleExperimental(out vec3 c[size * size]) {
  vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));
  ivec2 iCoord = ivec2(gl_FragCoord.xy) & 1;
  
  bool isLeft = iCoord.x == 0;
  bool isDown = iCoord.y == 0;

  {
    vec3 value1 = isLeft ? vec3(0) : textureLod(colorTex, surf.texCoord + vec2(-1, -1) * offset, 0).rgb;
    vec3 value2 = isLeft ? textureLod(colorTex, surf.texCoord + vec2(1, -1) * offset, 0).rgb : vec3(0);
    if (isLeft) {
      c[0] = textureLod(colorTex, surf.texCoord + vec2(-1, -1) * offset, 0).rgb;
      c[1] = dFdxFine(value1);
      c[2] = value2;
    } else {
      c[0] = value1;
      c[1] = -dFdxFine(value2);
      c[2] = textureLod(colorTex, surf.texCoord + vec2(1, -1) * offset, 0).rgb;
    }
  }
  
  {
    vec3 value3 = isLeft ? vec3(0) : textureLod(colorTex, surf.texCoord + vec2(-1, 0) * offset, 0).rgb;
    vec3 value4 = isLeft ? textureLod(colorTex, surf.texCoord + vec2(1, 0) * offset, 0).rgb : vec3(0);
    if (isLeft) {
      c[3] = textureLod(colorTex, surf.texCoord + vec2(-1, 0) * offset, 0).rgb;
      c[4] = dFdxFine(value3);
      c[5] = value4;
    } else {
      c[3] = value3;
      c[4] = -dFdxFine(value4);
      c[5] = textureLod(colorTex, surf.texCoord + vec2(1, 0) * offset, 0).rgb;
    }
  }

  {
    vec3 value5 = isLeft ? vec3(0) : textureLod(colorTex, surf.texCoord + vec2(-1, 1) * offset, 0).rgb;
    vec3 value6 = isLeft ? textureLod(colorTex, surf.texCoord + vec2(1, 1) * offset, 0).rgb : vec3(0);
    if (isLeft) {
      c[6] = textureLod(colorTex, surf.texCoord + vec2(-1, 1) * offset, 0).rgb;
      c[7] = dFdxFine(value5);
      c[8] = value6;
    } else {
      c[6] = value5;
      c[7] = -dFdxFine(value6);
      c[8] = textureLod(colorTex, surf.texCoord + vec2(1, 1) * offset, 0).rgb;
    }
  }
}

#ifdef MEDIAN
void sort(inout vec3 c[size * size]) {
  bool flag = true;
  for (int i = size * size - 1; flag && i > 0; --i) {
    flag = false;
    for (int j = 0; j < i; ++j) {
      if (dot(vec3(0.30, 0.59, 0.11), c[j]) > dot(vec3(0.30, 0.59, 0.11), c[j + 1])) {
        vec3 tmp = c[j];
        c[j] = c[j + 1];
        c[j + 1] = tmp;
        flag = true;
      } 
    }
  }
}
#endif

#ifdef BILATERAL
void getKernel(in vec3 c[size * size], out vec3 kernel[size * size]) {
  const float size2 = float(kernel.length());
  const vec3 h2 = h * h;
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      int index = j + distToBorder + (distToBorder + i) * size;
      vec3 colorDif2 = c[index] - c[size * size / 2];
      colorDif2 *= colorDif2;
      kernel[index] = exp(vec3(-float(i * i + j * j) / size2) - colorDif2 / h2);
    }
  }
}
#endif

void main() {
  vec3 c[size * size];
  // getSample(c);
  getSampleExperimental(c);

  #ifdef MEDIAN
  sort(c);
  color = vec4(c[size * size / 2], 1.0);
  #endif
  
  #ifdef BILATERAL
  vec3 kernel[size * size];
  getKernel(c, kernel);
  
  vec3 sum = vec3(0);
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      int index = j + distToBorder + (distToBorder + i) * size;
      color.rgb += c[index] * kernel[index];
      sum += kernel[index];
    }
  }
  color.rgb /= sum;
  color.a = 1.0;
  #endif
}