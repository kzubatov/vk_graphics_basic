#version 450
#extension GL_ARB_separate_shader_objects : enable

// #define MEDIAN
#define BILATERAL
const vec3 h = vec3(0.5); // bilateral filter: exp(-(i^2 + j^2) / size^2 - colorDif^2 / h^2)

layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

const int distToBorder = 2;
const int size = 2 * distToBorder + 1;

vec3 c[size * size];

void getSample() {
  vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));

  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      c[j + distToBorder + (distToBorder + i) * size] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
    }
  }
}

// Improves performance only for MEDIAN filter
void getSampleExperimental() {
  const vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));
  const ivec2 iCoord = ivec2(gl_FragCoord.xy) & 1;
  const int fragNum = iCoord.x + (iCoord.y << 1);
  const uint swapSize =  size * size - 1 >> 2;

  bool isLeft = iCoord.x == 0;
  bool isDown = iCoord.y == 0;
  
  vec3 leftToRightSwap[swapSize];
  vec3 rightToLeftSwap[swapSize];
  vec3 downToUpSwap[swapSize];
  vec3 upToDownSwap[swapSize];

  int vI, hI;
  int index;
  int swapIndex = 0;
  switch (fragNum) {
  case 0:
    c[0] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder) * offset, 0).rgb;
    vI = 2;
    hI = size << 1;
    for (int i = -distToBorder + 2; i <= distToBorder; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i, -distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[swapIndex] = c[vI];
      rightToLeftSwap[swapIndex] = vec3(0);
      downToUpSwap[swapIndex] = c[hI];
      upToDownSwap[swapIndex++] = vec3(0);
      vI += 2;
      hI += size << 1;
    }
    
    index = (size << 1) + 2;
    for (int i = -distToBorder + 2; i <= distToBorder; i += 2) {
      for (int j = -distToBorder + 2; j <= distToBorder; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapIndex] = c[index];
        rightToLeftSwap[swapIndex] = vec3(0);
        downToUpSwap[swapIndex] = c[index];
        upToDownSwap[swapIndex++] = vec3(0);
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 1:
    c[size - 1] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, -distToBorder) * offset, 0).rgb;
    vI = 0;
    hI = 3 * size - 1;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i, -distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, i + 2) * offset, 0).rgb;
      leftToRightSwap[swapIndex] = vec3(0);
      rightToLeftSwap[swapIndex] = c[vI];
      downToUpSwap[swapIndex] = c[hI];
      upToDownSwap[swapIndex++] = vec3(0);
      vI += 2;
      hI += size << 1;
    }

    index = size << 1;
    for (int i = -distToBorder + 2; i <= distToBorder; i += 2) {
      for (int j = -distToBorder; j <= distToBorder - 2; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapIndex] = vec3(0);
        rightToLeftSwap[swapIndex] = c[index];
        downToUpSwap[swapIndex] = c[index];
        upToDownSwap[swapIndex++] = vec3(0);
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 2:
    c[size * (size - 1)] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, distToBorder) * offset, 0).rgb;
    vI = size * (size - 1) + 2;
    hI = 0;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i + 2, distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[swapIndex] = c[vI];
      rightToLeftSwap[swapIndex] = vec3(0);
      downToUpSwap[swapIndex] = vec3(0);
      upToDownSwap[swapIndex++] = c[hI];
      vI += 2;
      hI += size << 1;
    }

    index = 2;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      for (int j = -distToBorder + 2; j <= distToBorder; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapIndex] = c[index];
        rightToLeftSwap[swapIndex] = vec3(0);
        downToUpSwap[swapIndex] = vec3(0);
        upToDownSwap[swapIndex++] = c[index];
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 3:
    c[size * size - 1] = textureLod(colorTex, surf.texCoord + vec2(distToBorder) * offset, 0).rgb;
    vI = size * (size - 1);
    hI = size - 1;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i, distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[swapIndex] = vec3(0);
      rightToLeftSwap[swapIndex] = c[vI];
      downToUpSwap[swapIndex] = vec3(0);
      upToDownSwap[swapIndex++] = c[hI];
      vI += 2;
      hI += size << 1;
    }

    index = 0;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      for (int j = -distToBorder; j <= distToBorder - 2; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapIndex] = vec3(0);
        rightToLeftSwap[swapIndex] = c[index];
        downToUpSwap[swapIndex] = vec3(0);
        upToDownSwap[swapIndex++] = c[index];
        index += 2;
      }
      index += size + 1;
    }
    break;
  }

  for (int i = 0; i < swapSize; ++i) {
    leftToRightSwap[i] = abs(dFdxFine(leftToRightSwap[i]));
    rightToLeftSwap[i] = abs(dFdxFine(rightToLeftSwap[i]));
    downToUpSwap[i] = abs(dFdyFine(downToUpSwap[i]));
    upToDownSwap[i] = abs(dFdyFine(upToDownSwap[i]));
  }

  swapIndex = 0;
  switch (fragNum) {
  case 0:
    vI = 1;
    hI = size;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      c[vI] = rightToLeftSwap[swapIndex];
      c[hI] = upToDownSwap[swapIndex++];
      vI += 2;
      hI += size << 1;
    }

    hI = size + 2;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      vI += size + 1;
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[vI] = rightToLeftSwap[swapIndex];
        c[hI] = upToDownSwap[swapIndex++];
        vI += 2;
        hI += 2;
      }
      hI += size + 1;
    }
    break;
  case 1:
    vI = 1;
    hI = (size << 1) - 1; 
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      c[vI] = leftToRightSwap[swapIndex];
      c[hI] = upToDownSwap[swapIndex++];
      vI += 2;
      hI += size << 1; 
    }

    hI = size;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      vI += size + 1;
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[vI] = leftToRightSwap[swapIndex];
        c[hI] = upToDownSwap[swapIndex++];
        vI += 2;
        hI += 2;
      }
      hI += size + 1;
    }
    break;
  case 2:
    vI = size * (size - 1) + 1;
    hI = size;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      c[vI] = rightToLeftSwap[swapIndex];
      c[hI] = downToUpSwap[swapIndex++];
      vI += 2;
      hI += size << 1;
    }

    vI = 1;
    hI = size + 2;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[vI] = rightToLeftSwap[swapIndex];
        c[hI] = downToUpSwap[swapIndex++];
        vI += 2;
        hI += 2;
      }
      vI += size + 1;
      hI += size + 1;
    }
    break;
  case 3:
    vI = size * (size - 1) + 1;
    hI = (size << 1) - 1;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      c[vI] = leftToRightSwap[swapIndex];
      c[hI] = downToUpSwap[swapIndex++];
      vI += 2;
      hI += size << 1;
    }

    vI = 1;
    hI = size;
    for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[vI] = leftToRightSwap[swapIndex];
        c[hI] = downToUpSwap[swapIndex++];
        vI += 2;
        hI += 2;
      }
      vI += size + 1;
      hI += size + 1;
    }
    break;
  }

  index = size + 1;
  for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
    for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
      c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
      index += 2;
    }
    index += size + 1;
  }
}

#ifdef MEDIAN
void sort() {
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
void getKernel(out vec3 kernel[size * size]) {
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
  getSample();
  // getSampleExperimental();

  #ifdef MEDIAN
  sort();
  color = vec4(c[size * size / 2], 1.0);
  #endif
  
  #ifdef BILATERAL
  vec3 kernel[size * size];
  getKernel(kernel);
  
  color = vec4(0,0,0,1);

  vec3 sum = vec3(0);
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      int index = j + distToBorder + (distToBorder + i) * size;
      color.rgb += c[index] * kernel[index];
      sum += kernel[index];
    }
  }
  color.rgb /= sum;
  #endif
}