#version 310 es
precision mediump float;
const int MAXL = 8;
in vec2 vUV;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vWorldPos;
in vec4 vLightSpacePos;
uniform sampler2D uTex;
uniform sampler2D uShadowMap;
uniform sampler2D uNormalMap;
uniform sampler2D uHeightMap;
uniform samplerCube uPointShadow;
uniform bool uHasPointShadow;
uniform vec3 uPointLightPos;
uniform float uPointFar;
uniform highp sampler2DArray uCSM;   // cascaded directional shadow
uniform mat4 uCSMMat[3];
uniform float uCSMSplit[3];
uniform samplerCube uEnv;            // procedural sky cubemap (image-based lighting)
uniform bool uHasNormalMap;
uniform float uParallax;
uniform float uAlpha;
uniform bool uIsTerrain;
uniform bool uIsWater;
uniform float uTime;
uniform vec3 uRockColor;
uniform vec3 uColor;
uniform float uShininess;
uniform float uSpecular;
uniform float uMetallic;
uniform float uRoughness;
uniform bool uSelected;
uniform vec3 uViewPos;
uniform int uNumLights;
uniform int uLightType[MAXL];
uniform vec3 uLightPos[MAXL];
uniform vec3 uLightColor[MAXL];
uniform float uLightIntensity[MAXL];
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal; // world-space normal (for SSAO/SSR)
float shadowFactor(vec3 n, vec3 L) {
    vec3 p = vLightSpacePos.xyz / vLightSpacePos.w;
    p = p * 0.5 + 0.5;
    if (p.z > 1.0) return 0.0;
    float bias = max(0.0025 * (1.0 - dot(n, L)), 0.0008);
    vec2 texel = vec2(1.0 / 1024.0);
    float sh = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closest = texture(uShadowMap, p.xy + vec2(float(x), float(y)) * texel).r;
            sh += (p.z - bias > closest) ? 1.0 : 0.0;
        }
    }
    return sh / 9.0;
}
// Cascaded directional shadow: pick a cascade by view distance, then PCF sample.
// Normal-offset + slope-scaled bias suppress acne without peter-panning.
float csmShadow(vec3 n, vec3 L) {
    float vd = length(vWorldPos - uViewPos);
    int layer = (vd < uCSMSplit[0]) ? 0 : (vd < uCSMSplit[1]) ? 1 : 2;
    float ndl = max(dot(n, L), 0.0);
    float offset = (0.02 + 0.05 * (1.0 - ndl)) * (1.0 + float(layer)); // wider for far cascades
    vec3 wp = vWorldPos + n * offset;
    vec4 lp = uCSMMat[layer] * vec4(wp, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0) return 0.0;
    float bias = max(0.0018 * (1.0 - ndl), 0.0006);
    vec2 texel = vec2(1.0 / 1024.0);
    float sh = 0.0;
    for (int x = -2; x <= 2; x++)
        for (int y = -2; y <= 2; y++) {
            float closest = texture(uCSM, vec3(p.xy + vec2(float(x), float(y)) * texel, float(layer))).r;
            sh += (p.z - bias > closest) ? 1.0 : 0.0;
        }
    return sh / 25.0; // 5x5 PCF
}
// Omnidirectional shadow for the chosen point light (cubemap of linear distance).
float pointShadow(vec3 worldPos) {
    vec3 toFrag = worldPos - uPointLightPos;
    float cur = length(toFrag) / uPointFar;
    float closest = texture(uPointShadow, toFrag).r;
    float bias = 0.02;
    return (cur - bias > closest) ? 1.0 : 0.0;
}
void main() {
    vec3 n = normalize(vNormal);
    vec2 uv = vUV;
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    if (uHasNormalMap) {
        vec3 T = normalize(vTangent - n * dot(n, vTangent));
        vec3 B = cross(n, T);
        mat3 TBN = mat3(T, B, n);
        if (uParallax > 0.0) {
            vec3 vT = transpose(TBN) * viewDir;
            float hgt = texture(uHeightMap, uv).r;
            uv = uv - (vT.xy / max(vT.z, 0.2)) * (hgt * uParallax);
        }
        vec3 nm = texture(uNormalMap, uv).rgb * 2.0 - 1.0;
        n = normalize(TBN * nm);
    }
    vec4 tex = texture(uTex, uv);
    vec3 base = tex.rgb * uColor;
    if (uIsTerrain) {
        // Blend flat (uColor) vs steep (uRockColor) by world-up slope.
        float slope = smoothstep(0.55, 0.9, normalize(vNormal).y);
        base = tex.rgb * mix(uRockColor, uColor, slope);
    }
    if (uIsWater) {
        // Animated ripples: perturb the up-normal by the gradient of a sum of
        // directional sine waves in world XZ, giving moving specular sparkle.
        vec2 p = vWorldPos.xz;
        vec2 d1 = vec2(0.80, 0.60), d2 = vec2(-0.60, 0.80), d3 = vec2(0.20, -0.98);
        float t = uTime;
        float dx = 0.055 * 0.9 * cos(dot(d1, p) * 0.9 + t * 1.3) * d1.x
                 + 0.040 * 1.7 * cos(dot(d2, p) * 1.7 + t * 1.9) * d2.x
                 + 0.028 * 3.1 * cos(dot(d3, p) * 3.1 + t * 2.6) * d3.x;
        float dz = 0.055 * 0.9 * cos(dot(d1, p) * 0.9 + t * 1.3) * d1.y
                 + 0.040 * 1.7 * cos(dot(d2, p) * 1.7 + t * 1.9) * d2.y
                 + 0.028 * 3.1 * cos(dot(d3, p) * 3.1 + t * 2.6) * d3.y;
        n = normalize(vec3(-dx, 1.0, -dz));
        base = uColor; // water tint drives the color; reflections add on top
    }
    // Cook-Torrance PBR (metallic/roughness).
    const float PI = 3.14159265;
    float rough = clamp(uRoughness, 0.05, 1.0);
    float a = rough * rough;
    vec3 F0 = mix(vec3(0.04), base, uMetallic);
    float NdotV = max(dot(n, viewDir), 0.001);
    // Image-based ambient from the sky cubemap (irradiance + rough reflection).
    vec3 envDiff = texture(uEnv, n).rgb;
    vec3 Refl = reflect(-viewDir, n);
    vec3 envSpec = mix(texture(uEnv, Refl).rgb, envDiff, rough); // rough => blurrier
    vec3 Fr = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - NdotV, 5.0);
    vec3 result = envDiff * base * (1.0 - uMetallic) * 0.5 + envSpec * Fr;
    for (int i = 0; i < uNumLights; i++) {
        vec3 L = (uLightType[i] == 0)
            ? normalize(uLightPos[i])
            : normalize(uLightPos[i] - vWorldPos);
        vec3 H = normalize(L + viewDir);
        float NdotL = max(dot(n, L), 0.0);
        float NdotH = max(dot(n, H), 0.0);
        float a2 = a * a;
        float dn = NdotH * NdotH * (a2 - 1.0) + 1.0;
        float D = a2 / (PI * dn * dn);
        float k = (rough + 1.0) * (rough + 1.0) / 8.0;
        float G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
        vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, viewDir), 0.0), 5.0);
        vec3 spec = (D * G) * F / max(4.0 * NdotV * NdotL, 0.001);
        vec3 kd = (vec3(1.0) - F) * (1.0 - uMetallic);
        vec3 lc = uLightColor[i] * uLightIntensity[i];
        float sh = (uLightType[i] == 0) ? csmShadow(n, L) : 0.0;
        if (uHasPointShadow && uLightType[i] == 1 &&
            distance(uLightPos[i], uPointLightPos) < 0.05) {
            sh = pointShadow(vWorldPos);
        }
        result += (kd * base / PI + spec) * lc * NdotL * (1.0 - sh);
    }
    if (uSelected) result += 0.3 * base + vec3(0.15);
    FragColor = vec4(result, tex.a * uAlpha);
    GNormal = vec4(normalize(n) * 0.5 + 0.5, 1.0);
}
