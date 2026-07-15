#version 310 es
precision highp float;
in vec2 vUV;
in vec3 vNormal;
uniform sampler2D uTex;
uniform vec3 uColor;
uniform float uMetallic;
uniform float uRoughness;
layout(location = 0) out vec4 gAlbedo; // rgb = albedo, a = metallic
layout(location = 1) out vec4 gNormal; // rgb = world normal, a = roughness
void main() {
    vec3 base = texture(uTex, vUV).rgb * uColor;
    gAlbedo = vec4(base, clamp(uMetallic, 0.0, 1.0));
    gNormal = vec4(normalize(vNormal) * 0.5 + 0.5, clamp(uRoughness, 0.05, 1.0));
}
