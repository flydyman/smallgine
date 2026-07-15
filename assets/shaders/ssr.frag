#version 310 es
precision highp float;
in vec2 vUV;
uniform sampler2D uScene;   // HDR color
uniform sampler2D uDepth;
uniform sampler2D uNormal;  // world normal
uniform mat4 uViewProj;
uniform mat4 uInvVP;
uniform vec3 uCamPos;
out vec4 FragColor;

vec3 worldFromDepth(vec2 uv, float d) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 wp = uInvVP * clip;
    return wp.xyz / wp.w;
}

void main() {
    float dep = texture(uDepth, vUV).r;
    if (dep >= 0.9999) { FragColor = vec4(0.0); return; }
    vec3 P = worldFromDepth(vUV, dep);
    vec3 N = normalize(texture(uNormal, vUV).xyz * 2.0 - 1.0);
    vec3 V = normalize(P - uCamPos);
    vec3 R = reflect(V, N);

    // Coarse world-space march, then binary-search refine the crossing.
    vec3 col = vec3(0.0);
    float hit = 0.0;
    vec3 step = R * 0.14;
    vec3 pos = P + N * 0.03, prev = pos;
    for (int i = 0; i < 32; i++) {
        prev = pos;
        pos += step;
        vec4 clip = uViewProj * vec4(pos, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        float sd = texture(uDepth, uv).r;
        vec3 scene = worldFromDepth(uv, sd);
        float rayDist = length(pos - uCamPos);
        float sceneDist = length(scene - uCamPos);
        if (rayDist > sceneDist + 0.02 && rayDist - sceneDist < 0.6) {
            // Binary-search between prev (in front) and pos (behind) for a sharp hit.
            vec3 a = prev, b = pos; vec2 huv = uv;
            for (int k = 0; k < 5; k++) {
                vec3 mid = (a + b) * 0.5;
                vec4 mc = uViewProj * vec4(mid, 1.0);
                huv = (mc.xy / mc.w) * 0.5 + 0.5;
                float md = texture(uDepth, huv).r;
                vec3 ms = worldFromDepth(huv, md);
                if (length(mid - uCamPos) > length(ms - uCamPos)) b = mid; else a = mid;
            }
            col = texture(uScene, huv).rgb;
            vec2 e = smoothstep(0.0, 0.15, huv) * smoothstep(0.0, 0.15, 1.0 - huv);
            float fres = pow(1.0 - max(dot(N, -V), 0.0), 3.0);
            float travel = 1.0 - float(i) / 32.0; // fade with march distance
            hit = e.x * e.y * (0.25 + 0.75 * fres) * travel;
            break;
        }
    }
    FragColor = vec4(col, hit);
}
