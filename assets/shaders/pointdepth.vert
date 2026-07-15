#version 310 es
layout(location = 0) in vec3 aPos;
uniform mat4 uVP;
uniform mat4 uModel;
out vec3 vWorld;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorld = wp.xyz;
    gl_Position = uVP * wp;
}
