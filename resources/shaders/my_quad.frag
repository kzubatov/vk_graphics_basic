#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MEDIAN
// #define BILATERAL
const vec3 h = vec3(0.5); // bilateral filter: exp(-(i^2 + j^2) / size^2 - colorDif^2 / h^2)

layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

const int distToBorder = 1;

const int size = 2 * distToBorder + 1;
const int size2 = size * size;

vec3 c[size2];

void getSample() {
  vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));

  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      c[j + distToBorder + (distToBorder + i) * size] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
    }
  }
}

void getSampleOddCol() {
  const vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));
  const int x = int(gl_FragCoord.x) & 1;
  const bool isLeft = x == 0;
  const uint swapSize =  size * (size - 1) >> 1;

  vec3 leftToRightSwap[swapSize];
  vec3 rightToLeftSwap[swapSize];
  
  int swapCounter = 0;
  int index = 0;
  if (isLeft) {
    for (int i = -distToBorder; i <= distToBorder; ++i) {
      c[index] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, i) * offset, 0).rgb;
      for (int j = -distToBorder + 2; j <= distToBorder; j += 2) {
        index += 2;
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapCounter] = c[index];
        rightToLeftSwap[swapCounter++] = vec3(0);
      }
      ++index;
    }
  } else {
    for (int i = -distToBorder; i <= distToBorder; ++i) {
      for (int j = -distToBorder; j <= distToBorder - 2; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[swapCounter] = vec3(0);
        rightToLeftSwap[swapCounter++] = c[index];
        index += 2;
      }
      c[index] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, i) * offset, 0).rgb;
      ++index;
    }
  }

  for (int i = 0; i < swapSize; ++i) {
    leftToRightSwap[i] = -dFdxFine(leftToRightSwap[i]);
    rightToLeftSwap[i] = dFdxFine(rightToLeftSwap[i]);
  }

  index = 1;
  swapCounter = 0;
  if (isLeft) {
    for (int i = -distToBorder; i <= distToBorder; ++i) {
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[index] = rightToLeftSwap[swapCounter++];
        index += 2;
      }
      ++index;
    }
  } else {
    for (int i = -distToBorder; i <= distToBorder; ++i) {
      for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
        c[index] = leftToRightSwap[swapCounter++];
        index += 2;
      }
      ++index;
    }
  }
}

void getSampleExperimental() {
  const vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));
  const ivec2 iCoord = ivec2(gl_FragCoord.xy) & 1;
  const int fragNum = iCoord.x + (iCoord.y << 1);
  const uint swapSize =  size2 - 1 >> 2;
  const int sizepsize = size << 1; 
  
  vec3 leftToRightSwap[swapSize];
  vec3 rightToLeftSwap[swapSize];
  vec3 downToUpSwap[swapSize];
  vec3 upToDownSwap[swapSize];
  
  int vI, hI;
  int borderSwapIndex = 0;
  int internalSwapIndex = size >> 1;

  int index = size + 1;
  for (int i = -distToBorder + 1; i <= distToBorder - 1; i += 2) {
    for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
      c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
      index += 2;
    }
    index += size + 1;
  }
  
  switch (fragNum) {
  case 0:
    c[0] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder) * offset, 0).rgb;
    vI = 2;
    hI = sizepsize;
    index = sizepsize + 2;

    for (int i = -distToBorder + 2; i <= distToBorder; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i, -distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[borderSwapIndex] = c[vI];
      rightToLeftSwap[borderSwapIndex] = vec3(0);
      downToUpSwap[borderSwapIndex] = c[hI];
      upToDownSwap[borderSwapIndex++] = vec3(0);
      vI += 2;
      hI += sizepsize;

      for (int j = -distToBorder + 2; j <= distToBorder; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[internalSwapIndex] = c[index];
        rightToLeftSwap[internalSwapIndex] = vec3(0);
        downToUpSwap[internalSwapIndex] = c[index];
        upToDownSwap[internalSwapIndex++] = vec3(0);
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 1:
    c[size - 1] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, -distToBorder) * offset, 0).rgb;
    vI = 0;
    hI = 3 * size - 1;
    index = sizepsize;
    for (int i = -distToBorder + 2; i <= distToBorder; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i - 2, -distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[borderSwapIndex] = vec3(0);
      rightToLeftSwap[borderSwapIndex] = c[vI];
      downToUpSwap[borderSwapIndex] = c[hI];
      upToDownSwap[borderSwapIndex++] = vec3(0);
      vI += 2;
      hI += sizepsize;
  
      for (int j = -distToBorder; j <= distToBorder - 2; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[internalSwapIndex] = vec3(0);
        rightToLeftSwap[internalSwapIndex] = c[index];
        downToUpSwap[internalSwapIndex] = c[index];
        upToDownSwap[internalSwapIndex++] = vec3(0);
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 2:
    c[size * (size - 1)] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, distToBorder) * offset, 0).rgb;
    vI = size * (size - 1) + 2;
    hI = 0;
    index = 2;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i + 2, distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(-distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[borderSwapIndex] = c[vI];
      rightToLeftSwap[borderSwapIndex] = vec3(0);
      downToUpSwap[borderSwapIndex] = vec3(0);
      upToDownSwap[borderSwapIndex++] = c[hI];
      vI += 2;
      hI += sizepsize;

      for (int j = -distToBorder + 2; j <= distToBorder; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[internalSwapIndex] = c[index];
        rightToLeftSwap[internalSwapIndex] = vec3(0);
        downToUpSwap[internalSwapIndex] = vec3(0);
        upToDownSwap[internalSwapIndex++] = c[index];
        index += 2;
      }
      index += size + 1;
    }
    break;
  case 3:
    c[size * size - 1] = textureLod(colorTex, surf.texCoord + vec2(distToBorder) * offset, 0).rgb;
    vI = size * (size - 1);
    hI = size - 1;
    index = 0;
    for (int i = -distToBorder; i <= distToBorder - 2; i += 2) {
      c[vI] = textureLod(colorTex, surf.texCoord + vec2(i, distToBorder) * offset, 0).rgb;
      c[hI] = textureLod(colorTex, surf.texCoord + vec2(distToBorder, i) * offset, 0).rgb;
      leftToRightSwap[borderSwapIndex] = vec3(0);
      rightToLeftSwap[borderSwapIndex] = c[vI];
      downToUpSwap[borderSwapIndex] = vec3(0);
      upToDownSwap[borderSwapIndex++] = c[hI];
      vI += 2;
      hI += sizepsize;
    
      for (int j = -distToBorder; j <= distToBorder - 2; j += 2) {
        c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
        leftToRightSwap[internalSwapIndex] = vec3(0);
        rightToLeftSwap[internalSwapIndex] = c[index];
        downToUpSwap[internalSwapIndex] = vec3(0);
        upToDownSwap[internalSwapIndex++] = c[index];
        index += 2;
      }
      index += size + 1;
    }
    break;
  }

  for (int i = 0; i < swapSize; ++i) {
    leftToRightSwap[i] = -dFdxFine(leftToRightSwap[i]);
    rightToLeftSwap[i] = dFdxFine(rightToLeftSwap[i]);
    downToUpSwap[i] = -dFdyFine(downToUpSwap[i]);
    upToDownSwap[i] = dFdyFine(upToDownSwap[i]);
  }

  int borderVI;
  int internalVI;
  int borderHI;
  int internalHI;
  borderSwapIndex = 0;
  internalSwapIndex = size >> 1;
  switch (fragNum) {
  case 0:
    borderVI = 1;
    internalVI = sizepsize + 1;

    borderHI = size;
    internalHI = size + 2;

    for (int i = distToBorder - 1; i >= 0; --i) {
      c[borderVI] = rightToLeftSwap[borderSwapIndex];
      c[borderHI] = upToDownSwap[borderSwapIndex++];
      borderVI += 2;
      borderHI += sizepsize;

      for (int j = distToBorder - 1; j >= 0; --j) {
        c[internalVI] = rightToLeftSwap[internalSwapIndex];
        c[internalHI] = upToDownSwap[internalSwapIndex++];
        internalVI += 2;
        internalHI += 2;
      }
      internalVI += size + 1;
      internalHI += size + 1;
    }
    break;
  case 1:
    borderVI = 1;
    internalVI = sizepsize + 1;

    borderHI = sizepsize - 1;
    internalHI = size;

    for (int i = distToBorder - 1; i >= 0; --i) {
      c[borderVI] = leftToRightSwap[borderSwapIndex];
      c[borderHI] = upToDownSwap[borderSwapIndex++];
      borderVI += 2;
      borderHI += sizepsize;

      for (int j = distToBorder - 1; j >= 0; --j) {
        c[internalVI] = leftToRightSwap[internalSwapIndex];
        c[internalHI] = upToDownSwap[internalSwapIndex++];
        internalVI += 2;
        internalHI += 2;
      }
      internalVI += size + 1;
      internalHI += size + 1;
    }
    break;
  case 2:
    borderVI = size * (size - 1) + 1;
    internalVI = 1;

    borderHI = size;
    internalHI = size + 2;

    for (int i = distToBorder - 1; i >= 0; --i) {
      c[borderVI] = rightToLeftSwap[borderSwapIndex];
      c[borderHI] = downToUpSwap[borderSwapIndex++];
      borderVI += 2;
      borderHI += sizepsize;

      for (int j = distToBorder - 1; j >= 0; --j) {
        c[internalVI] = rightToLeftSwap[internalSwapIndex];
        c[internalHI] = downToUpSwap[internalSwapIndex++];
        internalVI += 2;
        internalHI += 2;
      }
      internalVI += size + 1;
      internalHI += size + 1;
    }
    break;
  case 3:
    borderVI = size * (size - 1) + 1;
    internalVI = 1;

    borderHI = sizepsize - 1;
    internalHI = size;
    
    for (int i = distToBorder - 1; i >= 0; --i) {
      c[borderVI] = leftToRightSwap[borderSwapIndex];
      c[borderHI] = downToUpSwap[borderSwapIndex++];
      borderVI += 2;
      borderHI += sizepsize;

      for (int j = distToBorder - 1; j >= 0; --j) {
        c[internalVI] = leftToRightSwap[internalSwapIndex];
        c[internalHI] = downToUpSwap[internalSwapIndex++];
        internalVI += 2;
        internalHI += 2;
      }
      internalVI += size + 1;
      internalHI += size + 1;
    }
    break;
  }
}

#ifdef MEDIAN
void sort() {
  bool flag = true;
  for (int i = size2 - 1; flag && i > 0; --i) {
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
void getKernel(out vec3 kernel[size2]) {
  const vec3 h2 = h * h;
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder; j <= distToBorder; ++j) {
      int index = j + distToBorder + (distToBorder + i) * size;
      vec3 colorDif2 = c[index] - c[size2 / 2];
      colorDif2 *= colorDif2;
      kernel[index] = exp(vec3(-float(i * i + j * j) / float(size2)) - colorDif2 / h2);
    }
  }
}
#endif

void main() {
  // getSample();
  // getSampleOddCol();
  getSampleExperimental();
  #ifdef MEDIAN
  sort();
  color = vec4(c[size2 / 2], 1.0);
  return;
  #endif
  
  #ifdef BILATERAL
  vec3 kernel[size2];
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
  return;
  #endif
}