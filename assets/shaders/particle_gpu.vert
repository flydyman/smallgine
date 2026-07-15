#version 310 es
struct P { vec4 posLife; vec4 vel; };
layout(std430, binding = 0) buffer Buf { P p[]; };
uniform mat4 uViewProj;
out float vLife;
void main() {
    vec3 pos = p[gl_VertexID].posLife.xyz;
    vLife = p[gl_VertexID].posLife.w;
    gl_Position = uViewProj * vec4(pos, 1.0);
    gl_PointSize = 9.0;
}
