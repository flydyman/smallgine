#version 310 es
precision highp float;
const int MAXL = 8;
in vec2 vUV;
uniform sampler2D uAlbedo;
uniform sampler2D uNormalTex;
uniform sampler2D uDepth;
uniform mat4 uInvVP;
uniform vec3 uViewPos;
uniform int uNumLights;
uniform int uLightType[MAXL];
uniform vec3 uLightPos[MAXL];
uniform vec3 uLightColor[MAXL];
uniform float uLightIntensity[MAXL];
uniform highp sampler2DArray uCSM;
uniform mat4 uCSMMat[3];
uniform float uCSMSplit[3];
uniform samplerCube uPointShadow;
uniform bool uHasPointShadow;
uniform vec3 uPointLightPos;
uniform float uPointFar;
uniform samplerCube uEnv;   // sky cubemap (IBL)
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormalOut;

vec3 worldFromDepth(vec2 uv, float d) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 wp = uInvVP * clip;
    return wp.xyz / wp.w;
}
float csmShadow(vec3 P, vec3 n, vec3 L) {
    float vd = length(P - uViewPos);
    int layer = (vd < uCSMSplit[0]) ? 0 : (vd < uCSMSplit[1]) ? 1 : 2;
    float ndl = max(dot(n, L), 0.0);
    float offset = (0.02 + 0.05 * (1.0 - ndl)) * (1.0 + float(layer));
    vec4 lp = uCSMMat[layer] * vec4(P + n * offset, 1.0);
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
    return sh / 25.0;
}
float pointShadow(vec3 P) {
    vec3 toFrag = P - uPointLightPos;
    float cur = length(toFrag) / uPointFar;
    return (cur - 0.02 > texture(uPointShadow, toFrag).r) ? 1.0 : 0.0;
}
void main() {
    float dep = texture(uDepth, vUV).r;
    if (dep >= 0.9999) { FragColor = vec4(0.0); GNormalOut = vec4(0.5, 0.5, 1.0, 0.0); return; } // sky
    vec3 P = worldFromDepth(vUV, dep);
    vec4 alb = texture(uAlbedo, vUV);
    vec4 nrm = texture(uNormalTex, vUV);
    vec3 base = alb.rgb;
    float metallic = alb.a;
    vec3 n = normalize(nrm.xyz * 2.0 - 1.0);
    float rough = clamp(nrm.a, 0.05, 1.0);
    vec3 viewDir = normalize(uViewPos - P);

    const float PI = 3.14159265;
    float a = rough * rough;
    vec3 F0 = mix(vec3(0.04), base, metallic);
    float NdotV = max(dot(n, viewDir), 0.001);
    vec3 envDiff = texture(uEnv, n).rgb;
    vec3 Refl = reflect(-viewDir, n);
    vec3 envSpec = mix(texture(uEnv, Refl).rgb, envDiff, rough);
    vec3 Fr = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - NdotV, 5.0);
    vec3 result = envDiff * base * (1.0 - metallic) * 0.5 + envSpec * Fr;
    for (int i = 0; i < uNumLights; i++) {
        vec3 L = (uLightType[i] == 0) ? normalize(uLightPos[i]) : normalize(uLightPos[i] - P);
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
        vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 lc = uLightColor[i] * uLightIntensity[i];
        float sh = (uLightType[i] == 0) ? csmShadow(P, n, L) : 0.0;
        if (uHasPointShadow && uLightType[i] == 1 && distance(uLightPos[i], uPointLightPos) < 0.05)
            sh = pointShadow(P);
        result += (kd * base / PI + spec) * lc * NdotL * (1.0 - sh);
    }
    FragColor = vec4(result, 1.0);
    GNormalOut = vec4(n * 0.5 + 0.5, 1.0);
}
