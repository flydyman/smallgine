#version 310 es
precision highp float;
in vec2 vUV;
uniform sampler2D uDepth;
uniform sampler2D uNormal;   // world normal, rgb = n*0.5+0.5
uniform sampler2D uNoise;    // 4x4 rotation noise
uniform mat4 uViewProj;
uniform mat4 uInvVP;
uniform vec3 uCamPos;
uniform vec2 uNoiseScale;    // screen / 4
uniform float uRadius;
const int KERNEL = 16;
uniform vec3 uSamples[KERNEL];
out vec4 FragColor;

vec3 worldFromDepth(vec2 uv, float d) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 wp = uInvVP * clip;
    return wp.xyz / wp.w;
}

void main() {
    float dep = texture(uDepth, vUV).r;
    if (dep >= 0.9999) { FragColor = vec4(1.0); return; } // sky: no occlusion
    vec3 P = worldFromDepth(vUV, dep);
    vec3 N = normalize(texture(uNormal, vUV).xyz * 2.0 - 1.0);

    vec3 rvec = normalize(texture(uNoise, vUV * uNoiseScale).xyz * 2.0 - 1.0);
    vec3 T = normalize(rvec - N * dot(rvec, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occ = 0.0;
    float pDist = length(P - uCamPos);
    for (int i = 0; i < KERNEL; i++) {
        vec3 samp = P + (TBN * uSamples[i]) * uRadius;
        vec4 clip = uViewProj * vec4(samp, 1.0);
        vec2 suv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sd = texture(uDepth, suv).r;
        vec3 sceneP = worldFromDepth(suv, sd);
        float sceneDist = length(sceneP - uCamPos);
        float sampDist = length(samp - uCamPos);
        // Occluder is closer to camera than the sample point.
        float range = smoothstep(0.0, 1.0, uRadius / abs(pDist - sceneDist + 1e-4));
        if (sceneDist < sampDist - 0.02) occ += range;
    }
    float ao = 1.0 - occ / float(KERNEL);
    ao = pow(clamp(ao, 0.0, 1.0), 1.6); // punchier contact darkening
    FragColor = vec4(vec3(ao), 1.0);
}
