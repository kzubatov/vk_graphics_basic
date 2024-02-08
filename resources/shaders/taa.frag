#version 450
#extension GL_ARB_separate_shader_objects : enable

#define OPTIMIZED_METHOD
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
    // 3x3 ver 1
    /* vec4 coord = fsIn.texCoord.xyxy + t.xyxy * vec4(c_onePixel, -c_onePixel);

    vec3 tmp_h, tmp_v;

    mean = tmp_v = tmp_h = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);
    variance = tmp_v * tmp_v;

    tmp_v += dFdxFine(tmp_v) * t.x;
    mean += tmp_v;
    variance += tmp_v * tmp_v;

    currenColor = tmp_v += dFdyFine(tmp_v) * t.y;
    tmp_h += dFdyFine(tmp_h) * t.y;
    mean += tmp_h + tmp_v;
    variance += tmp_h * tmp_h + tmp_v * tmp_v;

    tmp_v = rgb2ycbcr(textureLod(currenFrame, coord.xw, 0).rgb);
    tmp_h = rgb2ycbcr(textureLod(currenFrame, coord.zy, 0).rgb);

    mean += tmp_h + tmp_v;
    variance += tmp_h * tmp_h + tmp_v * tmp_v;
    
    tmp_v += dFdyFine(tmp_v) * t.y;
    tmp_h += dFdyFine(tmp_h) * t.y;
    mean += tmp_h + tmp_v;
    variance += tmp_h * tmp_h + tmp_v * tmp_v;
    
    tmp_v = rgb2ycbcr(textureLod(currenFrame, coord.zw, 0).rgb);
    mean += tmp_v;
    variance += tmp_v * tmp_v;
    return; */

    // 3x3 ver 2
    /* vec4 coord = fsIn.texCoord.xyxy + t.xyxy * vec4(c_onePixel, -c_onePixel);

    mean = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);
    variance = mean * mean;

    mean += currenColor = mean + dFdxFine(mean) * t.x;
    variance += variance + dFdxFine(variance) * t.x;

    currenColor += dFdyFine(currenColor) * t.y; 
    mean += mean + dFdyFine(mean) * t.y;
    variance += variance + dFdyFine(variance) * t.y;

    vec3 tmp_v = rgb2ycbcr(textureLod(currenFrame, coord.xw, 0).rgb);
    vec3 tmp_h = rgb2ycbcr(textureLod(currenFrame, coord.zy, 0).rgb);

    mean += (tmp_v + tmp_h) * 2.0 + dFdxFine(tmp_v) * t.x + dFdyFine(tmp_h) * t.y;
    tmp_h *= tmp_h; tmp_v *= tmp_v;
    variance += (tmp_v + tmp_h) * 2.0 + dFdxFine(tmp_v) * t.x + dFdyFine(tmp_h) * t.y;
                
    tmp_v = rgb2ycbcr(textureLod(currenFrame, coord.zw, 0).rgb);
    mean += tmp_v;
    variance += tmp_v * tmp_v;
    return; */

    // 3x3 ver 3
    vec4 coord = fsIn.texCoord.xyxy + vec4(c_onePixel, -c_onePixel);

    mean = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);
    variance = mean * mean;

    vec3 texel_0 = rgb2ycbcr(textureLod(currenFrame, coord.zw, 0).rgb);
    vec3 texel_1 = rgb2ycbcr(textureLod(currenFrame, coord.xw, 0).rgb); 
    vec3 texel_2 = rgb2ycbcr(textureLod(currenFrame, coord.zy, 0).rgb);
    vec3 texel_3 = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);

    mean = texel_0 + texel_1 + texel_2 + texel_3;
    variance = texel_0 * texel_0 + texel_1 * texel_1 + texel_2 * texel_2 + texel_3 * texel_3;

    x_swap(texel_1, texel_0, currenColor, isOdd.x);
    y_swap(texel_2, texel_0, texel_0, isOdd.y);
    x_swap(texel_3, texel_2, texel_2, isOdd.x);
    y_swap(texel_3, texel_1, texel_3, isOdd.y);

    mean += currenColor + texel_0 + texel_2 + texel_3;
    variance += currenColor * currenColor + texel_0 * texel_0 + texel_2 * texel_2 + texel_3 * texel_3;

    y_swap(texel_2, currenColor, currenColor, isOdd.y);
    mean += currenColor;
    variance += currenColor * currenColor; 
    return;
#elif WINDOW == 2
    // 5x5 ver 1
    vec4 coord = fsIn.texCoord.xyxy + t.xyxy * vec4(c_twoPixels, -c_twoPixels);

    currenColor = mean = rgb2ycbcr(textureLod(currenFrame, fsIn.texCoord, 0).rgb);
    variance = mean * mean;

    vec3 tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord.x, fsIn.texCoord.y), 0).rgb);
    mean += tmp;
    variance += tmp * tmp;

    tmp = rgb2ycbcr(textureLod(currenFrame, vec2(fsIn.texCoord.x, coord.y), 0).rgb);
    mean += tmp;
    variance += tmp * tmp;
    
    tmp = rgb2ycbcr(textureLod(currenFrame, coord.xy, 0).rgb);
    mean += tmp;
    variance += tmp * tmp;

    mean += mean + dFdxFine(mean) * t.x;
    variance += variance + dFdxFine(variance) * t.x;

    mean += mean + dFdyFine(mean) * t.y;
    variance += variance + dFdyFine(variance) * t.y;

    vec3 tmp_mean;
    vec3 tmp_variance;

    tmp_mean = tmp = rgb2ycbcr(textureLod(currenFrame, vec2(fsIn.texCoord.x, coord.w), 0).rgb);
    tmp_variance = tmp * tmp;
    
    tmp_mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord.xw), 0).rgb);
    tmp_variance += tmp * tmp;

    mean += tmp_mean + tmp_mean + dFdxFine(tmp_mean) * t.x;
    variance += tmp_variance + tmp_variance + dFdxFine(tmp_variance) * t.x;

    tmp_mean = tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord.z, fsIn.texCoord.y), 0).rgb);
    tmp_variance = tmp * tmp;
    
    tmp_mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord.zy), 0).rgb);
    tmp_variance += tmp * tmp;

    mean += tmp_mean + tmp_mean + dFdyFine(tmp_mean) * t.y;
    variance += tmp_variance + tmp_variance + dFdyFine(tmp_variance) * t.y;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord.zw), 0).rgb);
    variance += tmp * tmp;
    return;
#elif WINDOW == 3
    // ok maybe it has no sense but why not (at least it has +fps and may be useful in 16k on mobile devices in 2040)
    // 7x7 ver 1
    vec4 coord_onePixel = fsIn.texCoord.xyxy + t.xyxy * vec4(c_onePixel, -c_onePixel);
    vec4 coord_threePixel = fsIn.texCoord.xyxy + 3.0 * t.xyxy * vec4(c_onePixel, -c_onePixel);
    vec3 tmp;
    
    mean = tmp = rgb2ycbcr(textureLod(currenFrame, coord_onePixel.zw, 0).rgb);
    variance = tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, coord_onePixel.xw, 0).rgb);
    variance += tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_threePixel.x, coord_onePixel.w), 0).rgb);
    variance += tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, coord_onePixel.zy, 0).rgb);
    variance += tmp * tmp;

    mean += currenColor = tmp = rgb2ycbcr(textureLod(currenFrame, coord_onePixel.xy, 0).rgb);
    variance += tmp * tmp;

    currenColor += dFdxFine(currenColor) * t.x;
    currenColor += dFdyFine(currenColor) * t.y;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_threePixel.x, coord_onePixel.y), 0).rgb);
    variance += tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_onePixel.z, coord_threePixel.y), 0).rgb);
    variance += tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_onePixel.x, coord_threePixel.y), 0).rgb);
    variance += tmp * tmp;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, coord_threePixel.xy, 0).rgb);
    variance += tmp * tmp;

    mean += mean + dFdxFine(mean) * t.x;
    variance += variance + dFdxFine(variance) * t.x;

    mean += mean + dFdyFine(mean) * t.y;
    variance += variance + dFdyFine(variance) * t.y;
    
    vec3 mean_tmp, variance_tmp;
    mean_tmp = tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_onePixel.z, coord_threePixel.w), 0).rgb);
    variance_tmp = tmp * tmp;

    mean_tmp += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_onePixel.x, coord_threePixel.w), 0).rgb);
    variance_tmp += tmp * tmp;

    mean_tmp += tmp = rgb2ycbcr(textureLod(currenFrame, coord_threePixel.xw, 0).rgb);
    variance_tmp += tmp * tmp;

    mean += mean_tmp + mean_tmp + dFdxFine(mean_tmp) * t.x;
    variance += variance_tmp + variance_tmp + dFdxFine(variance_tmp) * t.x;

    mean_tmp = tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_threePixel.z, coord_onePixel.w), 0).rgb);
    variance_tmp = tmp * tmp;

    mean_tmp += tmp = rgb2ycbcr(textureLod(currenFrame, vec2(coord_threePixel.z, coord_onePixel.y), 0).rgb);
    variance_tmp += tmp * tmp;

    mean_tmp += tmp = rgb2ycbcr(textureLod(currenFrame, coord_threePixel.zy, 0).rgb);
    variance_tmp += tmp * tmp;

    mean += mean_tmp + mean_tmp + dFdyFine(mean_tmp) * t.y;
    variance += variance_tmp + variance_tmp + dFdyFine(variance_tmp) * t.y;

    mean += tmp = rgb2ycbcr(textureLod(currenFrame, coord_threePixel.zw, 0).rgb);
    variance += tmp * tmp;
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
    alpha = clamp(max(t.x, max(t.y, t.z)), 0.0, 1.0);
    prevColor = ycbcr2rgb(mix(prevColor, currenColor, alpha));

    return ycbcr2rgb(currenColor);
}

void main() 
{
    bool isMoving = bool(textureLod(stencilMap, fsIn.texCoord, 0).r);
    vec2 prev_uv;

    if (isMoving) {
        prev_uv = textureLod(velocityBuffer, fsIn.texCoord, 0).rg;
    } else {
        vec4 p = vec4(2.0 * fsIn.texCoord - 1.0, textureLod(depthMap, fsIn.texCoord, 0).r, 1.0);
        p = params.mPrevInvCur * p;
        prev_uv = p.xy / p.w * 0.5 + 0.5;
    }

    float alpha = 0.9 * float(prev_uv.x <= 1.0 && prev_uv.x >= 0.0 && prev_uv.y <= 1.0 && prev_uv.y >= 0.0);
    alpha *= max(1.0 - length(prev_uv - fsIn.texCoord) * 0.5, 0.0);

    vec3 prevColor = BicubicLagrangeTextureSample(prev_uv);
    vec3 currenColor = varianceClipping(prevColor);

    color = vec4(mix(currenColor, prevColor, alpha), 1.0);
}