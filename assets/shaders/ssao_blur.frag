#version 310 es
precision highp float;
in vec2 vUV;
uniform sampler2D uTex;    // raw AO
uniform sampler2D uDepth;  // scene depth (bilateral weight)
uniform vec2 uTexel;       // 1/size
out vec4 FragColor;
void main() {
    // Depth-aware (bilateral) 4x4 blur: hides noise without bleeding across edges.
    float dc = texture(uDepth, vUV).r;
    float sum = 0.0, wsum = 0.0;
    for (int x = -2; x < 2; x++)
        for (int y = -2; y < 2; y++)
        {
            vec2 o = vUV + vec2(float(x), float(y)) * uTexel;
            float d = texture(uDepth, o).r;
            float w = exp(-abs(d - dc) * 400.0);
            sum += texture(uTex, o).r * w;
            wsum += w;
        }
    FragColor = vec4(vec3(sum / max(wsum, 1e-4)), 1.0);
}
