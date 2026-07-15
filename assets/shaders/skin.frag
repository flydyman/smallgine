#version 310 es
precision mediump float;
in vec3 vNormal;
in vec3 vWorld;
uniform vec3 uLightDir;
uniform vec3 uColor;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal;
void main() {
    vec3 n = normalize(vNormal);
    float d = max(dot(n, normalize(uLightDir)), 0.0);
    vec3 c = uColor * (0.2 + 0.85 * d);
    FragColor = vec4(c, 1.0);
    GNormal = vec4(n * 0.5 + 0.5, 1.0);
}
