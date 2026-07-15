#version 310 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uScene;
out vec4 FragColor;
void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(l > 0.75 ? c : vec3(0.0), 1.0);
}
