#version 310 es
precision highp float;
in vec2 vUV;
uniform sampler2D uScene;   // HDR scene
uniform sampler2D uBloom;   // blurred bright pass
uniform sampler2D uDof;     // blurred full scene (depth of field)
uniform sampler2D uDepth;   // scene depth
uniform float uNear;
uniform float uFar;
uniform float uTime;
uniform mat4 uInvVP;        // inverse(proj*view)
uniform mat4 uPrevVP;       // previous frame proj*view
out vec4 FragColor;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float linDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
    float dep = texture(uDepth, vUV).r;
    bool isSky = dep >= 0.9999;

    // Reconstruct world position, then previous-frame screen position => velocity.
    vec4 clip = vec4(vUV * 2.0 - 1.0, dep * 2.0 - 1.0, 1.0);
    vec4 wp = uInvVP * clip; wp /= wp.w;
    vec4 pc = uPrevVP * vec4(wp.xyz, 1.0); pc /= pc.w;
    vec2 prevUV = pc.xy * 0.5 + 0.5;
    vec2 vel = clamp((vUV - prevUV) * 0.5, vec2(-0.02), vec2(0.02));

    // Motion blur along velocity + chromatic aberration.
    vec3 c = vec3(0.0);
    const int MB = 4;
    for (int i = 0; i < MB; i++) {
        vec2 uv = vUV - vel * (float(i) / float(MB));
        vec2 dir = uv - 0.5;
        float ca = 0.003;
        c.r += texture(uScene, uv + dir * ca).r;
        c.g += texture(uScene, uv).g;
        c.b += texture(uScene, uv - dir * ca).b;
    }
    c /= float(MB);

    // Depth of field: blend blurred scene by circle-of-confusion.
    float dist = isSky ? uFar : linDepth(dep);
    float coc = clamp(abs(dist - 6.0) / 8.0, 0.0, 1.0);
    c = mix(c, texture(uDof, vUV).rgb, coc * 0.55);

    // Bloom.
    c += texture(uBloom, vUV).rgb * 1.1;

    // Distance fog.
    float fog = isSky ? 0.0 : (1.0 - exp(-0.018 * max(dist - 4.0, 0.0)));
    c = mix(c, vec3(0.72, 0.58, 0.52), clamp(fog, 0.0, 0.55));

    // Tonemap + gamma.
    c *= 1.1;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));

    // Vignette + saturation.
    float d2 = distance(vUV, vec2(0.5));
    c *= mix(0.45, 1.0, smoothstep(0.85, 0.35, d2));
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, 1.15);

    // Film grain + dither.
    c += (hash(vUV * vec2(1280.0) + fract(uTime)) - 0.5) * 0.05;

    FragColor = vec4(c, 1.0);
}
