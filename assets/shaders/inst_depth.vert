#version 310 es
layout(location = 0) in vec3 aPos;
layout(location = 4) in vec4 aM0;
layout(location = 5) in vec4 aM1;
layout(location = 6) in vec4 aM2;
layout(location = 7) in vec4 aM3;
uniform mat4 uLightSpace;
void main() {
    mat4 model = mat4(aM0, aM1, aM2, aM3);
    gl_Position = uLightSpace * model * vec4(aPos, 1.0);
}
