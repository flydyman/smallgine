#version 310 es
precision mediump float;
in vec3 vDir;
uniform samplerCube uSky;
out vec4 FragColor;
void main() { FragColor = texture(uSky, normalize(vDir)); }
