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
uniform bool uHasNormalMap;
uniform float uParallax;
uniform float uAlpha;
uniform vec3 uColor;
uniform float uShininess;
uniform float uSpecular;
uniform bool uSelected;
uniform vec3 uViewPos;
uniform int uNumLights;
uniform int uLightType[MAXL];
uniform vec3 uLightPos[MAXL];
uniform vec3 uLightColor[MAXL];
uniform float uLightIntensity[MAXL];
out vec4 FragColor;
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
    vec3 result = 0.15 * base;
    for (int i = 0; i < uNumLights; i++) {
        vec3 L = (uLightType[i] == 0)
            ? normalize(uLightPos[i])
            : normalize(uLightPos[i] - vWorldPos);
        float diff = max(dot(n, L), 0.0);
        vec3 h = normalize(L + viewDir);
        float spec = pow(max(dot(n, h), 0.0), uShininess) * uSpecular;
        vec3 lc = uLightColor[i] * uLightIntensity[i];
        float sh = (uLightType[i] == 0) ? shadowFactor(n, L) : 0.0;
        result += lc * (1.0 - sh) * (diff * base + spec);
    }
    if (uSelected) result += 0.3 * base + vec3(0.15);
    FragColor = vec4(result, tex.a * uAlpha);
}
