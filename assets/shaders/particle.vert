#version 310 es
layout(location = 0) in vec3 aPos;       // unit quad corner (-0.5..0.5)
layout(location = 4) in vec4 aInst0;     // xyz = world position, w = size
layout(location = 5) in vec4 aInst1;     // rgb = color, a = life (0..1)
uniform mat4 uViewProj;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aPos.xy + 0.5;
    vColor = aInst1;
    float s = aInst0.w;
    vec3 world = aInst0.xyz + (uCamRight * aPos.x + uCamUp * aPos.y) * s;
    gl_Position = uViewProj * vec4(world, 1.0);
}
