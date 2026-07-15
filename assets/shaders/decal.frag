#version 310 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec3 uTint;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal;
void main() {
    vec4 t = texture(uTex, vUV);
    FragColor = vec4(uTint * t.rgb, t.a);
    GNormal = vec4(0.5, 0.5, 1.0, t.a); // up-ish; ground decals barely perturb SSAO
}
