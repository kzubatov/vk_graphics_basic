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

const int distToBorder = 2;

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
  int borderIndex = isLeft ? 0 : size - 1;
  int index = isLeft ? 2 : 0;
  int jBegin = isLeft ? -distToBorder + 2 : -distToBorder;
  int jEnd = isLeft ? distToBorder : distToBorder - 2;
  int borderX = isLeft ? -distToBorder : distToBorder;
  
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    c[borderIndex] = textureLod(colorTex, surf.texCoord + vec2(borderX, i) * offset, 0).rgb;
    borderIndex += size;
    for (int j = jBegin; j <= jEnd; j += 2) {
      c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
      leftToRightSwap[swapCounter] = isLeft ? c[index] : vec3(0);
      rightToLeftSwap[swapCounter++] = isLeft ? vec3(0) : c[index];
      index += 2;
    }
    ++index;
  }
  
  for (int i = 0; i < swapSize; ++i) {
    leftToRightSwap[i] = -dFdxFine(leftToRightSwap[i]);
    rightToLeftSwap[i] = dFdxFine(rightToLeftSwap[i]);
  }

  index = 1;
  swapCounter = 0;
  for (int i = -distToBorder; i <= distToBorder; ++i) {
    for (int j = -distToBorder + 1; j <= distToBorder - 1; j += 2) {
      c[index] = isLeft ? rightToLeftSwap[swapCounter++] : leftToRightSwap[swapCounter++];
      index += 2;
    }
    ++index;
  }
}

void getSampleExperimental() {
  const vec2 offset = vec2(dFdxFine(surf.texCoord.x), dFdyFine(surf.texCoord.y));
  const ivec2 iCoord = ivec2(gl_FragCoord.xy) & 1;
  const bool isLeft = iCoord.x == 0;
  const bool isDown = iCoord.y == 0;
  const uint swapSize =  size2 - 1 >> 2;
  const int sizepsize = size << 1; 
  
  vec3 leftToRightSwap[swapSize];
  vec3 rightToLeftSwap[swapSize];
  vec3 downToUpSwap[swapSize];
  vec3 upToDownSwap[swapSize];
  
  int vI = (isLeft ? 2 : 0) + (isDown ? 0 : size * (size - 1));
  int hI = (isLeft ? 0 : size - 1) + (isDown ? sizepsize : 0);
  int index = (isLeft ? 2 : 0) + (isDown ? sizepsize : 0);
  int borderSwapIndex = 0;
  int internalSwapIndex = size >> 1;
  int iBegin = isDown ? -distToBorder + 2 : -distToBorder;
  int iEnd = isDown ? distToBorder : distToBorder - 2;
  int jBegin = isLeft ? -distToBorder + 2 : -distToBorder;
  int jEnd = isLeft ? distToBorder : distToBorder - 2;

  vec2 vBorderVertexOffset = isDown ? vec2(isLeft ? 0 : -2, -distToBorder) : vec2(isLeft ? 2 : 0, distToBorder);
  vec2 hBorderVertexOffset = vec2(isLeft ? -distToBorder : distToBorder, 0);

  int separateIndex = size + 1;
  for (int i = iBegin; i <= iEnd; i += 2) {
    c[vI] = textureLod(colorTex, surf.texCoord + (vec2(i, 0) + vBorderVertexOffset) * offset, 0).rgb;
    c[hI] = textureLod(colorTex, surf.texCoord + (vec2(0, i) + hBorderVertexOffset) * offset, 0).rgb;
    leftToRightSwap[borderSwapIndex] = isLeft ? c[vI] : vec3(0);
    rightToLeftSwap[borderSwapIndex] = isLeft ? vec3(0) : c[vI];
    downToUpSwap[borderSwapIndex] = isDown ? c[hI] : vec3(0);
    upToDownSwap[borderSwapIndex++] = isDown ? vec3(0) : c[hI];
    vI += 2;
    hI += sizepsize;

    for (int j = jBegin; j <= jEnd; j += 2) {
      c[separateIndex] = textureLod(colorTex, surf.texCoord + vec2(j - 1 + (iCoord.x << 1), i - 1 + (iCoord.y << 1)) * offset, 0).rgb;
      c[index] = textureLod(colorTex, surf.texCoord + vec2(j, i) * offset, 0).rgb;
      leftToRightSwap[internalSwapIndex] = isLeft ? c[index] : vec3(0);
      rightToLeftSwap[internalSwapIndex] = isLeft ? vec3(0) : c[index];
      downToUpSwap[internalSwapIndex] = isDown ? c[index] : vec3(0);
      upToDownSwap[internalSwapIndex++] = isDown ? vec3(0) : c[index];
      index += 2;
      separateIndex += 2;
    }
    index += size + 1;
    separateIndex += size + 1;
  }

  separateIndex = (isLeft ? 0 : size - 1) + (isDown ? 0 : size * (size - 1));
  vec2 separateVertex = vec2(isLeft ? -distToBorder : distToBorder, isDown ? -distToBorder : distToBorder);
  c[separateIndex] = textureLod(colorTex, surf.texCoord + separateVertex * offset, 0).rgb;


  for (int i = 0; i < swapSize; ++i) {
    leftToRightSwap[i] = -dFdxFine(leftToRightSwap[i]);
    rightToLeftSwap[i] = dFdxFine(rightToLeftSwap[i]);
    downToUpSwap[i] = -dFdyFine(downToUpSwap[i]);
    upToDownSwap[i] = dFdyFine(upToDownSwap[i]);
  }

  int borderVI = isDown ? 1 : size * (size - 1) + 1;
  int internalVI = isDown ? sizepsize + 1 : 1;
  int borderHI = isLeft ? size : sizepsize - 1;
  int internalHI = isLeft ? size + 2 : size;
  borderSwapIndex = 0;
  internalSwapIndex = size >> 1;

  for (int i = distToBorder - 1; i >= 0; --i) {
    c[borderVI] = isLeft ? rightToLeftSwap[borderSwapIndex] : leftToRightSwap[borderSwapIndex];
    c[borderHI] = isDown ? upToDownSwap[borderSwapIndex++] : downToUpSwap[borderSwapIndex++];
    borderVI += 2;
    borderHI += sizepsize;

    for (int j = distToBorder - 1; j >= 0; --j) {
      c[internalVI] = isLeft ? rightToLeftSwap[borderSwapIndex] : leftToRightSwap[borderSwapIndex];
      c[internalHI] = isDown ? upToDownSwap[borderSwapIndex++] : downToUpSwap[borderSwapIndex++];
      internalVI += 2;
      internalHI += 2;
    }
    internalVI += size + 1;
    internalHI += size + 1;
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