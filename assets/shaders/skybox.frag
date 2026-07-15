#version 310 es
precision mediump float;
in vec3 vDir;
uniform samplerCube uSky;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal;
void main() { FragColor = texture(uSky, normalize(vDir)); GNormal = vec4(0.5, 0.5, 1.0, 0.0); }
