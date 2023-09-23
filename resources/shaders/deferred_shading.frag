#version 450
#define MAX_LIGHTS_PER_FRAG 48
const float PI = 3.1415;    

layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D zBuffer;

layout (location = 0) in vec2 texCoord;

layout(push_constant) uniform PushConstant {
  mat4 projMatInv;
  mat4 viewMatInv;
} constants;

layout(location = 0) out vec4 outColor;

struct LightInfo {
  vec4 position;
  vec4 colorAndRadius;
};

layout (std430, binding = 3) readonly buffer lights {
  LightInfo lightInfo[];
};

layout (std430, binding = 4) readonly buffer visible_lights {
  uint count;
  uint id[];
};

layout(binding = 5) uniform common_info {
  vec4 cameraPos;
  uint lightCount;
  uint width;
} commonInfo;

vec3 fresnel(vec3 F0, float nv) {
    return mix(F0, vec3(1), pow(1.0 - nv, 5.0));
}

float G_Neumann(float nl, float nv) {
    return nl * nv / max(0.01, max(nl, nv));
}

float G_CookTorrance(float nl, float nv, float nh, float vh) {
    return min(1.0, min(2.0 * nh * nv / vh, 2.0 * nh * nl / vh));
}

float G_Implicit(float nl, float nv) {
    return nl * nv;
}

float D_Beckmann(float roughness, float nh) {
    float nh2 = max(0.01, nh * nh);
    float tmp = nh2 * roughness * roughness;
    return 1.0 / tmp / nh2 * exp((nh2 - 1.0) / tmp);
}

float CookTorrance(float nl, float nv, float nh, float vh, float roughness) {
    return D_Beckmann(roughness, nh) * G_CookTorrance(nl, nv, nh, vh);
}

void main() {
  vec4 tmp = texture(colorMap, texCoord).rgba;
  vec3 color = pow(tmp.rgb, vec3(2.2));
  float metalness = tmp.a;

  tmp = texture(normalMap, texCoord).rgba;
  vec3 normal = tmp.rgb;
  float roughness = tmp.a;
  
  float z = texture(zBuffer, texCoord).r;
  vec4 clipSpacePosition = vec4(texCoord * 2.0 - vec2(1.0), z, 1.0);
  vec4 viewSpacePosition = constants.projMatInv * clipSpacePosition;
  viewSpacePosition /= viewSpacePosition.w;
  vec3 wPos = vec3(constants.viewMatInv * viewSpacePosition);

  vec3 v = normalize(commonInfo.cameraPos.xyz - wPos);
  float nv = max(0.0, dot(normal, v));
  vec3 F0 = mix(vec3(0.04), color, metalness);
  vec3 specFresnel = max(vec3(0.0), fresnel(F0, nv));
  
  vec3 resColor = vec3(0);

  uvec2 fragCoord = uvec2(gl_FragCoord);
  uint idx = fragCoord.x + fragCoord.y * commonInfo.width;

  bool insideSphere;
  uint lightCount = 0;
  uint lightID[MAX_LIGHTS_PER_FRAG];

  for (uint i = 0; i < count && lightCount < MAX_LIGHTS_PER_FRAG; ++i) {
    tmp.xyz = wPos - lightInfo[id[i]].position.xyz;
    insideSphere = dot(tmp.xyz, tmp.xyz) <= lightInfo[id[i]].colorAndRadius.w * lightInfo[id[i]].colorAndRadius.w;
    if (insideSphere) {
      lightID[lightCount++] = id[i];
    }
  }

  vec3 lightPos;
  vec3 lightColor;
  for (uint i = 0; i < lightCount; ++i) {
    lightPos = lightInfo[lightID[i]].position.xyz;
    lightColor = lightInfo[lightID[i]].colorAndRadius.rgb;

    vec3 l = normalize(lightPos - wPos);
    vec3 h = normalize(l + v);

    float nl = max(0.0, dot(normal, l));
    float nh = max(0.0, dot(normal, h));
    float vh = max(0.0, dot(v, h));

    vec3 spec = specFresnel * CookTorrance(nl, nv, nh, vh, roughness) / max(0.01, 4.0 * nv);
    vec3 diff = max(vec3(0), (vec3(1) - specFresnel) * nl / PI);

    resColor += (diff * mix(color, vec3(0), metalness) + spec) * lightColor;
  }

  outColor = vec4(pow(resColor, vec3(1.0 / 2.2)), 1.0);
}