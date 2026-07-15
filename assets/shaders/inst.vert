#version 310 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 4) in vec4 aM0;
layout(location = 5) in vec4 aM1;
layout(location = 6) in vec4 aM2;
layout(location = 7) in vec4 aM3;
uniform mat4 uViewProj;
uniform mat4 uLightSpace;
out vec3 vNormal;
out vec2 vUV;
out vec4 vLS;
void main() {
    mat4 model = mat4(aM0, aM1, aM2, aM3);
    vNormal = mat3(model) * aNormal;
    vUV = aUV;
    vec4 wp = model * vec4(aPos, 1.0);
    vLS = uLightSpace * wp;
    gl_Position = uViewProj * wp;
}
