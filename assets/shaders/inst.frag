#version 310 es
precision mediump float;
in vec3 vNormal;
in vec2 vUV;
in vec4 vLS;
uniform sampler2D uTex;
uniform sampler2D uShadow;
uniform vec3 uLightDir;
out vec4 FragColor;
float shadow(vec3 n) {
    vec3 p = vLS.xyz / vLS.w * 0.5 + 0.5;
    if (p.z > 1.0) return 0.0;
    float bias = max(0.004 * (1.0 - dot(n, uLightDir)), 0.0015);
    return (p.z - bias > texture(uShadow, p.xy).r) ? 1.0 : 0.0;
}
void main() {
    vec3 n = normalize(vNormal);
    float d = max(dot(n, uLightDir), 0.0);
    float sh = shadow(n);
    vec3 c = texture(uTex, vUV).rgb * (0.3 + 0.7 * d * (1.0 - sh));
    FragColor = vec4(c, 1.0);
}
