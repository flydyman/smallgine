#version 310 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightSpace;
out vec2 vUV;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vWorldPos;
out vec4 vLightSpacePos;
void main() {
    vUV = aUV;
    mat3 nm = mat3(transpose(inverse(uModel)));
    vNormal = nm * aNormal;
    vTangent = nm * aTangent;
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vLightSpacePos = uLightSpace * wp;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
