#version 310 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent; // unused in deferred (no normal mapping)
uniform mat4 uMVP;
uniform mat4 uModel;
out vec2 vUV;
out vec3 vNormal;
void main() {
    vUV = aUV;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
