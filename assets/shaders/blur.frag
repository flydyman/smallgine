#version 310 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir;
out vec4 FragColor;
void main() {
    float w[5];
    w[0]=0.227027; w[1]=0.194594; w[2]=0.121621; w[3]=0.054054; w[4]=0.016216;
    vec3 c = texture(uTex, vUV).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uTex, vUV + uDir * float(i)).rgb * w[i];
        c += texture(uTex, vUV - uDir * float(i)).rgb * w[i];
    }
    FragColor = vec4(c, 1.0);
}
