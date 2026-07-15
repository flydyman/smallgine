#version 310 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uInvRes;
out vec4 FragColor;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
    vec3 m  = texture(uTex, vUV).rgb;
    float lM = luma(m);
    float lNW = luma(texture(uTex, vUV + vec2(-1.0,-1.0)*uInvRes).rgb);
    float lNE = luma(texture(uTex, vUV + vec2( 1.0,-1.0)*uInvRes).rgb);
    float lSW = luma(texture(uTex, vUV + vec2(-1.0, 1.0)*uInvRes).rgb);
    float lSE = luma(texture(uTex, vUV + vec2( 1.0, 1.0)*uInvRes).rgb);
    float lMin = min(lM, min(min(lNW,lNE), min(lSW,lSE)));
    float lMax = max(lM, max(max(lNW,lNE), max(lSW,lSE)));
    vec2 dir = vec2(-((lNW+lNE)-(lSW+lSE)), ((lNW+lSW)-(lNE+lSE)));
    float reduce = max((lNW+lNE+lSW+lSE)*0.03125, 0.0078125);
    float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir*rcp, -8.0, 8.0) * uInvRes;
    vec3 rA = 0.5*(texture(uTex, vUV+dir*(1.0/3.0-0.5)).rgb + texture(uTex, vUV+dir*(2.0/3.0-0.5)).rgb);
    vec3 rB = rA*0.5 + 0.25*(texture(uTex, vUV-dir*0.5).rgb + texture(uTex, vUV+dir*0.5).rgb);
    float lB = luma(rB);
    FragColor = vec4((lB < lMin || lB > lMax) ? rA : rB, 1.0);
}
