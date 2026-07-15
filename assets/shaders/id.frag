#version 310 es
precision highp float;
uniform vec3 uID;   // node index encoded as RGB (bytes/255)
out vec4 FragColor;
void main() { FragColor = vec4(uID, 1.0); }
