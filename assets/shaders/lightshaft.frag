#version 310 es
precision highp float;
in vec2 vUV;
uniform sampler2D uDepth;   // sky (far depth) = light source, geometry = occluder
uniform vec2 uSunUV;        // sun position in screen space
uniform float uSunVisible;  // 1 when the sun is on-screen and in front
out vec4 FragColor;
void main() {
    if (uSunVisible < 0.5) { FragColor = vec4(0.0); return; }
    const int N = 48;
    vec2 delta = (uSunUV - vUV) / float(N);
    vec2 uv = vUV;
    float illum = 1.0, sum = 0.0;
    for (int i = 0; i < N; i++) {
        uv += delta;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        float d = texture(uDepth, uv).r;
        float mask = (d >= 0.9999) ? 1.0 : 0.0; // unoccluded sky lets light through
        sum += mask * illum;
        illum *= 0.96; // decay along the ray
    }
    FragColor = vec4(vec3(sum / float(N)), 1.0);
}
