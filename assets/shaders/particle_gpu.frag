#version 310 es
precision mediump float;
in float vLife;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 GNormal;
void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) discard;
    float a = (1.0 - d) * clamp(vLife * 0.6, 0.0, 1.0);
    vec3 col = mix(vec3(0.2, 0.5, 1.0), vec3(0.6, 0.9, 1.0), clamp(vLife * 0.4, 0.0, 1.0));
    FragColor = vec4(col * a, a);   // additive
    GNormal = vec4(0.5, 0.5, 1.0, a);
}
