#version 450
#extension GL_ARB_separate_shader_objects : enable

// #define OPTIMIZED_METHOD
#define WINDOW 1

#ifdef OPTIMIZED_METHOD
#extension GL_GOOGLE_include_directive : require
#include "diff_swap.h"
#endif

layout(location = 0) out vec4 color;

layout(location = 0) in FS_IN 
{
    vec2 texCoord;
} fsIn;

layout(binding = 0) uniform sampler2D depthMap;
layout(binding = 1) uniform usampler2D stencilMap;
layout(binding = 2) uniform sampler2D velocityBuffer;
layout(binding = 3) uniform sampler2D historyBuffer;
layout(binding = 4) uniform sampler2D currenFrame;

layout(push_constant) uniform params_t
{
    mat4 mPrevInvCur;
    vec2 resolution;
} params;

vec2 c_onePixel = 1.0 / params.resolution;
vec2 c_twoPixels = 2.0 / params.resolution;

float c_x0 = -1.0;
float c_x1 =  0.0;
float c_x2 =  1.0;
float c_x3 =  2.0;

vec3 CubicLagrange (vec3 A, vec3 B, vec3 C, vec3 D, float t)
{
    return
        A * 
        (
            (t - c_x1) / (c_x0 - c_x1) * 
            (t - c_x2) / (c_x0 - c_x2) *
            (t - c_x3) / (c_x0 - c_x3)
        ) +
        B * 
        (
            (t - c_x0) / (c_x1 - c_x0) * 
            (t - c_x2) / (c_x1 - c_x2) *
            (t - c_x3) / (c_x1 - c_x3)
        ) +
        C * 
        (
            (t - c_x0) / (c_x2 - c_x0) * 
            (t - c_x1) / (c_x2 - c_x1) *
            (t - c_x3) / (c_x2 - c_x3)
        ) +       
        D * 
        (
            (t - c_x0) / (c_x3 - c_x0) * 
            (t - c_x1) / (c_x3 - c_x1) *
            (t - c_x2) / (c_x3 - c_x2)
        );
}

vec3 BicubicLagrangeTextureSample (vec2 P)
{
    vec2 pixel = P * params.resolution + 0.5;
    
    vec2 frac = fract(pixel);
    pixel = floor(pixel) / params.resolution - vec2(c_onePixel/2.0);
    
    vec3 C00 = textureLod(historyBuffer, pixel + vec2(-c_onePixel.x ,-c_onePixel.y), 0).rgb;
    vec3 C10 = textureLod(historyBuffer, pixel + vec2( 0.0        ,-c_onePixel.y), 0).rgb;
    vec3 C20 = textureLod(historyBuffer, pixel + vec2( c_onePixel.x ,-c_onePixel.y), 0).rgb;
    vec3 C30 = textureLod(historyBuffer, pixel + vec2( c_twoPixels.x,-c_onePixel.y), 0).rgb;
    
    vec3 C01 = textureLod(historyBuffer, pixel + vec2(-c_onePixel.x , 0.0), 0).rgb;
    vec3 C11 = textureLod(historyBuffer, pixel + vec2( 0.0        , 0.0), 0).rgb;
    vec3 C21 = textureLod(historyBuffer, pixel + vec2( c_onePixel.x , 0.0), 0).rgb;
    vec3 C31 = textureLod(historyBuffer, pixel + vec2( c_twoPixels.x, 0.0), 0).rgb;    
    
    vec3 C02 = textureLod(historyBuffer, pixel + vec2(-c_onePixel.x , c_onePixel.y), 0).rgb;
    vec3 C12 = textureLod(historyBuffer, pixel + vec2( 0.0        , c_onePixel.y), 0).rgb;
    vec3 C22 = textureLod(historyBuffer, pixel + vec2( c_onePixel.x , c_onePixel.y), 0).rgb;
    vec3 C32 = textureLod(historyBuffer, pixel + vec2( c_twoPixels.x, c_onePixel.y), 0).rgb;    
    
    vec3 C03 = textureLod(historyBuffer, pixel + vec2(-c_onePixel.x , c_twoPixels.y), 0).rgb;
    vec3 C13 = textureLod(historyBuffer, pixel + vec2( 0.0        , c_twoPixels.y), 0).rgb;
    vec3 C23 = textureLod(historyBuffer, pixel + vec2( c_onePixel.x , c_twoPixels.y), 0).rgb;
    vec3 C33 = textureLod(historyBuffer, pixel + vec2( c_twoPixels.x, c_twoPixels.y), 0).rgb;    
    
    vec3 CP0X = CubicLagrange(C00, C10, C20, C30, frac.x);
    vec3 CP1X = CubicLagrange(C01, C11, C21, C31, frac.x);
    vec3 CP2X = CubicLagrange(C02, C12, C22, C32, frac.x);
    vec3 CP3X = CubicLagrange(C03, C13, C23, C33, frac.x);
    
    return CubicLagrange(CP0X, CP1X, CP2X, CP3X, frac.y);
}

vec3 rgb2ycbcr(vec3 rgb) 
{   
    vec3 ycbcr;
    ycbcr.x = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    ycbcr.y = -0.1146 * rgb.r - 0.3854 * rgb.g + 0.5 * rgb.b;
    ycbcr.z = 0.5 * rgb.r - 0.4542 * rgb.g - 0.0458 * rgb.b;
    return ycbcr;
}

vec3 ycbcr2rgb(vec3 ycbcr) 
{
    vec3 rbg;
    rbg.x = ycbcr.x + 1.5748 * ycbcr.z;
    rbg.y = ycbcr.x - 0.1873 * ycbcr.y - 0.4681 * ycbcr.z;
    rbg.z = ycbcr.x + 1.8556 * ycbcr.y;
    return rbg;
}

#ifdef OPTIMIZED_METHOD
void getVarianceClippingInfo(out vec3 currenColor, out vec3 mean, out vec3 variance)
{
    const bvec2 isOdd = bvec2(ivec2(gl_FragCoord.xy) & 1);
    const vec2 t = vec2(isOdd) * -2.0 + 1.0;

#if WINDOW == 1
    // 3x3, interpolation is optimal for this area size
    
    // use nonuniform sampling to avoid ternary operators
    vec4 coord = fsIn.texCoord.xyxy + t.xyxy * vec4(c_onePixel, -c_onePixel);

    // get two texels from the row that will be interpolated for our neighbor along y-axis
    vec3 texel_0 = rgb2ycbcr(textureLod(currenFrame, coord.zy, 0).rgb); 
    // this texel can be exchanged along the x-axis
    vec3 texel_1 = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);
    
    // get partial sums 
    mean = texel_0 + texel_1;
    variance = texel_0 * texel_0 + texel_1 * texel_1;

    // get middle texel for this row using swap along x-axis
    texel_1 += dFdxFine(texel_1) * t.x;

    mean += texel_1;
    variance += texel_1 * texel_1;

    // get central (for local area) texel using swap along y-axis
    currenColor = texel_1 + dFdyFine(texel_1) * t.y;

    // interpolate partial sums for the neighbor along y-axis
    mean += mean + dFdyFine(mean) * t.y;
    variance += variance + dFdyFine(variance) * t.y;

    // get two texels from the row that won't be interpolated for our neighbor along y-axis
    texel_0 = rgb2ycbcr(textureLod(currenFrame, coord.zw, 0).rgb); 
    // this texel can be exchanged along the x-axis
    texel_1 = rgb2ycbcr(textureLod(currenFrame, coord.xw, 0).rgb);

    mean += texel_0 + texel_1;
    variance += texel_0 * texel_0 + texel_1 * texel_1;
    
    // get middle texel for this row using swap along x-axis
    texel_1 += dFdxFine(texel_1) * t.x;

    // complete partial sums
    mean += texel_1;
    variance += texel_1 * texel_1;
    return;
#elif WINDOW == 2
    // 5x5, nonuniform sampling and transfer of partial sums are optimal for this and larger area sizes

    // nonuniform sampling
    vec3 texels[9];
    texels[0] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-2, -2), 0).rgb);
    texels[1] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 0, -2), 0).rgb);
    texels[2] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 2, -2), 0).rgb);
    texels[3] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-2,  0), 0).rgb);
    texels[4] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord, 0).rgb);
    texels[5] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 2,  0), 0).rgb);
    texels[6] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-2,  2), 0).rgb);
    texels[7] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 0,  2), 0).rgb);
    texels[8] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 2,  2), 0).rgb);

    currenColor = texels[4];

    variance = mean = vec3(0);
    
    // Compute partial sums for our y-axis neighbor
    for (int i = 3; i <= 8; ++i)
    {
        mean += texels[i];
        variance += texels[i] * texels[i];
    }

    // Perform swap along x-axis
    mean += dFdxFine(mean) * t.x;
    variance += dFdxFine(variance) * t.x;

    // and contribute a part in diagonal neighbor’s sums
    mean += texels[4] + texels[5] + texels[7] + texels[8];
    variance += texels[4] * texels[4] + texels[5] * texels[5] + texels[7] * texels[7] + texels[8] * texels[8];

    // Perform swap along y-axis
    mean += dFdyFine(mean) * t.y;
    variance += dFdyFine(variance) * t.y;

    // And contribute a part in x-axis neighbor’s sums
    mean += texels[1] + texels[2] + texels[4] + texels[5] + texels[7] + texels[8];
    variance += texels[1] * texels[1] + texels[2] * texels[2] + texels[4] * texels[4]
        + texels[5] * texels[5] + texels[7] * texels[7] + texels[8] * texels[8];

    // Perform swap along x-axis
    mean += dFdxFine(mean) * t.x;
    variance += dFdxFine(variance) * t.x;

    //And contribute a part in sums of fragment itself:
    for (int i = 0; i <= 8; ++i)
    {
        mean += texels[i];
        variance += texels[i] * texels[i];
    }
    
    return;
#elif WINDOW == 3
    // ok maybe it has no sense but why not (at least it has +fps and may be useful in 16k on mobile devices in 2040)
    // 7x7

    // nonuniform sampling
    vec3 texels[16];
    texels[ 0] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-3, -3), 0).rgb);
    texels[ 1] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-1, -3), 0).rgb);
    texels[ 2] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 1, -3), 0).rgb);
    texels[ 3] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 3, -3), 0).rgb);
    texels[ 4] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-3, -1), 0).rgb);
    texels[ 5] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-1, -1), 0).rgb);
    texels[ 6] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 1, -1), 0).rgb);
    texels[ 7] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 3, -1), 0).rgb);
    texels[ 8] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-3,  1), 0).rgb);
    texels[ 9] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-1,  1), 0).rgb);
    texels[10] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 1,  1), 0).rgb);
    texels[11] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 3,  1), 0).rgb);
    texels[12] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-3,  3), 0).rgb);
    texels[13] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2(-1,  3), 0).rgb);
    texels[14] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 1,  3), 0).rgb);
    texels[15] = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + t * c_onePixel * vec2( 3,  3), 0).rgb);

    variance = mean = vec3(0);
    
    // Compute partial sums for our y-axis neighbor
    for (int i = 4; i <= 15; ++i)
    {
        mean += texels[i];
        variance += texels[i] * texels[i];
    }

    // Perform swap along x-axis
    mean += dFdxFine(mean) * t.x;
    variance += dFdxFine(variance) * t.x;

    // and contribute a part in diagonal neighbor’s sums
    mean += texels[5] + texels[6] + texels[7] + texels[9] 
        + texels[10] + texels[11] + texels[13] + texels[14] + texels[15];
    variance += texels[5] * texels[5] + texels[6] * texels[6] + texels[7] * texels[7] 
        + texels[9] * texels[9] + texels[10] * texels[10] + texels[11] * texels[11] 
        + texels[13] * texels[13] + texels[14] * texels[14] + texels[15] * texels[15];

    // Perform swap along y-axis
    mean += dFdyFine(mean) * t.y;
    variance += dFdyFine(variance) * t.y;

    // And contribute a part in x-axis neighbor’s sums
    mean += texels[1] + texels[2] + texels[3] + texels[5] + texels[6] 
        + texels[7] + texels[9] + texels[10] + texels[11] + texels[13] + texels[14] + texels[15];
    variance += texels[1] * texels[1] + texels[2] * texels[2] + texels[3] * texels[3] 
        + texels[5] * texels[5] + texels[6] * texels[6] + texels[7] * texels[7] 
        + texels[9] * texels[9] + texels[10] * texels[10] + texels[11] * texels[11] 
        + texels[13] * texels[13] + texels[14] * texels[14] + texels[15] * texels[15];
    
    // Perform swap along x-axis
    mean += dFdxFine(mean) * t.x;
    variance += dFdxFine(variance) * t.x;

    //And contribute a part in sums of fragment itself:
    for (int i = 0; i <= 15; ++i)
    {
        mean += texels[i];
        variance += texels[i] * texels[i];
    }
    
    // get current color
    currenColor = texels[10];
    currenColor += dFdxFine(currenColor) * t.x;
    currenColor += dFdyFine(currenColor) * t.y;
    return;
#endif
}
#endif

vec3 varianceClipping(inout vec3 prevColor)
{
    vec3 mean = vec3(0);
    vec3 variance = vec3(0);
    vec3 currenColor;

#ifdef OPTIMIZED_METHOD
    getVarianceClippingInfo(currenColor, mean, variance);
#else
    for (int i = -WINDOW; i <= WINDOW; ++i)
    {
        for (int j = -WINDOW; j <= WINDOW; ++j)
        {
            vec3 tmp = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord + vec2(i, j) * c_onePixel, 0).rgb);
            if (i == 0 && j == 0) currenColor = tmp;
            mean += tmp;
            variance += tmp * tmp;
        }
    }
#endif

#if WINDOW == 1
    mean /= 9.0f;
    variance = sqrt(variance / 9.0f - mean * mean);
#elif WINDOW == 2
    mean /= 25.0f;
    variance = sqrt(variance / 25.0f - mean * mean);
#elif WINDOW == 3
    mean /= 49.0f;
    variance = sqrt(variance / 49.0f - mean * mean);
#endif

    vec3 minC = mean - variance;
    vec3 maxC = mean + variance;

    prevColor = rgb2ycbcr(prevColor);
    vec3 rayDir = currenColor - prevColor;
    vec3 t = min((minC - prevColor) / rayDir, (maxC - prevColor) / rayDir);
    float alpha = clamp(max(t.x, max(t.y, t.z)), 0.0, 1.0);
    prevColor = ycbcr2rgb(mix(prevColor, currenColor, alpha));

    return ycbcr2rgb(currenColor);
}

void main() 
{
    bool isMoving = bool(textureLod(stencilMap, fsIn.texCoord, 0).r);
    vec2 prev_uv;

    if (isMoving) {
        prev_uv = fsIn.texCoord - textureLod(velocityBuffer, fsIn.texCoord, 0).rg;
    } else {
        vec4 p = vec4(2.0 * fsIn.texCoord - 1.0, textureLod(depthMap, fsIn.texCoord, 0).r, 1.0);
        p = params.mPrevInvCur * p;
        prev_uv = p.xy / p.w * 0.5 + 0.5;
    }

    float alpha = 0.95 * float(prev_uv.x <= 1.0 && prev_uv.x >= 0.0 && prev_uv.y <= 1.0 && prev_uv.y >= 0.0);
    alpha *= max(1.0 - length(prev_uv - fsIn.texCoord) * 0.5, 0.0);
    color = vec4(vec3(alpha), 1.0);

    vec3 prevColor = BicubicLagrangeTextureSample(prev_uv);
    vec3 currenColor = varianceClipping(prevColor);

    color = vec4(mix(currenColor, prevColor, alpha), 1.0);
}