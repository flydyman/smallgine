#version 310 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aJoints;   // joint indices (x,y used)
layout(location = 4) in vec4 aWeights;  // blend weights
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat4 uJoints[32];                // per-joint skinning matrices
out vec3 vNormal;
out vec3 vWorld;
void main() {
    // Linear blend skinning over up to four influencing joints.
    mat4 skin = aWeights.x * uJoints[int(aJoints.x)]
              + aWeights.y * uJoints[int(aJoints.y)]
              + aWeights.z * uJoints[int(aJoints.z)]
              + aWeights.w * uJoints[int(aJoints.w)];
    vec4 sp = skin * vec4(aPos, 1.0);
    vec3 sn = mat3(skin) * aNormal;
    vec4 wp = uModel * sp;
    vWorld = wp.xyz;
    vNormal = mat3(uModel) * sn;
    gl_Position = uViewProj * wp;
}
