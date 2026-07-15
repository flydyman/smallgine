#version 310 es
layout(location = 0) in vec3 aPos;    // quad in XZ plane, [-0.5,0.5]
layout(location = 4) in vec4 aInst;   // xyz = world position, w = size
uniform mat4 uViewProj;
out vec2 vUV;
void main() {
    vUV = aPos.xz + 0.5;
    vec3 world = aInst.xyz + vec3(aPos.x, 0.0, aPos.z) * aInst.w;
    world.y += 0.02; // sit just above the surface
    gl_Position = uViewProj * vec4(world, 1.0);
}
