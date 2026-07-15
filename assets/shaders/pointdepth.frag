#version 310 es
precision highp float;
in vec3 vWorld;
uniform vec3 uLightPos;
uniform float uFar;
out vec4 FragColor;
void main() {
    // Store normalized distance from the light (linear), sampled by lit.frag.
    FragColor = vec4(length(vWorld - uLightPos) / uFar, 0.0, 0.0, 1.0);
}
