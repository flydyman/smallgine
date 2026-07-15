#version 310 es
precision mediump float;
in vec2 vUV;
in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal;
void main() {
    // Soft round sprite; fade to edge and over lifetime.
    float d = length(vUV - 0.5) * 2.0;
    float a = smoothstep(1.0, 0.0, d) * vColor.a;
    FragColor = vec4(vColor.rgb * a, a); // premultiplied, additive
    GNormal = vec4(0.5, 0.5, 1.0, 0.0);
}
