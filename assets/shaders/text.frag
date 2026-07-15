#version 310 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uFont;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    float a = texture(uFont, vUV).r;
    FragColor = vec4(uColor, a);
}
