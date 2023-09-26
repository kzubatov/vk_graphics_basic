#version 450

layout(std430, push_constant) uniform params_t
{
  mat4 mProjView;
  uint meshID;
  vec3 cameraPos;
} params;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normal;

layout(set = 1, binding = 0) uniform sampler2D bunnyAlbedo;
layout(set = 1, binding = 1) uniform sampler2D bunnyNormal;
layout(set = 1, binding = 2) uniform sampler2D bunnyRoughness;

layout(set = 2, binding = 0) uniform sampler2D teapotAlbedo;
layout(set = 2, binding = 1) uniform sampler2D teapotNormal;
layout(set = 2, binding = 2) uniform sampler2D teapotMetalness;
layout(set = 2, binding = 3) uniform sampler2D teapotRoughness;

layout(set = 3, binding = 0) uniform sampler2D flatAlbedo;
layout(set = 3, binding = 1) uniform sampler2D flatNormal;
layout(set = 3, binding = 2) uniform sampler2D flatRoughness;
layout(set = 3, binding = 3) uniform sampler2D flatHeight;

layout (location = 0) in FS_IN
{
  vec3 wPos;
  vec3 wTangent;
  vec3 wBitangent;
  vec3 wNorm;
  vec2 texCoord;
  vec3 tPos;
  vec3 tView;
} fsIn;

// vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) {
//   // number of depth layers
//   const float minLayers = 128;
//   const float maxLayers = 768;
//   float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
//   // calculate the size of each layer
//   float layerDepth = 1.0 / numLayers;
//   // depth of current layer
//   float currentLayerDepth = 0.0;
//   // the amount to shift the texture coordinates per layer (from vector P)
//   vec2 P = viewDir.xy / viewDir.z / 6;
//   vec2 deltaTexCoords = P / numLayers;

//   // get initial values
//   vec2  currentTexCoords     = texCoords;
//   float currentDepthMapValue = texture(flatHeight, currentTexCoords).r;
  
//   while(currentLayerDepth < currentDepthMapValue)
//   {
//       // shift texture coordinates along direction of P
//     currentTexCoords -= deltaTexCoords;
//     // get depthmap value at current texture coordinates
//     currentDepthMapValue = texture(flatHeight, currentTexCoords).r;  
//     // get depth of next layer
//     currentLayerDepth += layerDepth;
//   }
    
//   // get texture coordinates before collision (reverse operations)
//   vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

//   // get depth after and before collision for linear interpolation
//   float afterDepth  = currentDepthMapValue - currentLayerDepth;
//   float beforeDepth = texture(flatHeight, prevTexCoords).r - currentLayerDepth + layerDepth;
 
//   // interpolation of texture coordinates
//   float weight = afterDepth / (afterDepth - beforeDepth);
//   vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

//   return finalTexCoords;
// }

void main() {
  vec3 wT = normalize(fsIn.wTangent);
  vec3 wB = normalize(fsIn.wBitangent);
  vec3 wN = normalize(fsIn.wNorm);

  vec3 tViewDir = normalize(fsIn.tView - fsIn.tPos);
  vec2 tex = fsIn.texCoord;

  vec3 c;
  vec3 n;
  float metalness;
  float roughness;
  switch (params.meshID) {
  case 0:
    c = texture(bunnyAlbedo, tex).rgb;
    n = texture(bunnyNormal, tex).rgb * 2.0 - 1.0;
    metalness = 0.0;
    roughness = texture(bunnyRoughness, tex).r;
    break;
  case 1:
    c = texture(teapotAlbedo, tex).rgb;
    n = texture(teapotNormal, tex).rgb * 2.0 - 1.0;
    metalness = texture(teapotMetalness, tex).r;
    roughness = texture(teapotRoughness, tex).r;
    break;    
  case 2:
    // tex = parallaxMapping(4.0 * tex, tViewDir);
    c = texture(flatAlbedo, 8 * tex).rgb;
    n = texture(flatNormal, 8 * tex).rgb * 2.0 - 1.0;
    metalness = 0.0;
    roughness = texture(flatRoughness, 8 * tex).r;
    break;
  }

  color = vec4(c, metalness);
  n = normalize(n.r * wT + n.g * wB + n.b * wN);
  normal = vec4(n * 0.5 + vec3(0.5), roughness);
}
