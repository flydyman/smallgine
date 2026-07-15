#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <iostream>

namespace smallgine {

namespace {
    const char* kFsVert =
        "#version 310 es\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
        "    vUV = p;\n"
        "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";

    // Bright-pass: keep only luminance above a threshold.
    const char* kBrightFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uScene;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    vec3 c = texture(uScene, vUV).rgb;\n"
        "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
        "    FragColor = vec4(l > 0.75 ? c : vec3(0.0), 1.0);\n"
        "}\n";

    // Separable Gaussian blur (direction in texels).
    const char* kBlurFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec2 uDir;\n" // texel-sized offset along blur axis
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    float w[5];\n"
        "    w[0]=0.227027; w[1]=0.194594; w[2]=0.121621; w[3]=0.054054; w[4]=0.016216;\n"
        "    vec3 c = texture(uTex, vUV).rgb * w[0];\n"
        "    for (int i = 1; i < 5; i++) {\n"
        "        c += texture(uTex, vUV + uDir * float(i)).rgb * w[i];\n"
        "        c += texture(uTex, vUV - uDir * float(i)).rgb * w[i];\n"
        "    }\n"
        "    FragColor = vec4(c, 1.0);\n"
        "}\n";

    // Composite: scene + bloom, then gamma / vignette / saturation.
    const char* kCompositeFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uScene;\n"
        "uniform sampler2D uBloom;\n"
        "out vec4 FragColor;\n"
        "vec3 aces(vec3 x) {\n"
        "    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);\n"
        "}\n"
        "void main() {\n"
        "    vec3 c = texture(uScene, vUV).rgb;\n"          // HDR
        "    c += texture(uBloom, vUV).rgb * 1.1;\n"        // additive bloom
        "    c *= 1.1;\n"                                   // exposure
        "    c = aces(c);\n"                                // HDR -> LDR tonemap
        "    c = pow(c, vec3(1.0 / 2.2));\n"                // gamma
        "    float d = distance(vUV, vec2(0.5));\n"
        "    c *= mix(0.45, 1.0, smoothstep(0.85, 0.35, d));\n" // vignette
        "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
        "    c = mix(vec3(l), c, 1.15);\n"                  // saturation
        "    FragColor = vec4(c, 1.0);\n"
        "}\n";

    const char* kFxaaFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec2 uInvRes;\n"
        "out vec4 FragColor;\n"
        "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
        "void main() {\n"
        "    vec3 m  = texture(uTex, vUV).rgb;\n"
        "    float lM = luma(m);\n"
        "    float lNW = luma(texture(uTex, vUV + vec2(-1.0,-1.0)*uInvRes).rgb);\n"
        "    float lNE = luma(texture(uTex, vUV + vec2( 1.0,-1.0)*uInvRes).rgb);\n"
        "    float lSW = luma(texture(uTex, vUV + vec2(-1.0, 1.0)*uInvRes).rgb);\n"
        "    float lSE = luma(texture(uTex, vUV + vec2( 1.0, 1.0)*uInvRes).rgb);\n"
        "    float lMin = min(lM, min(min(lNW,lNE), min(lSW,lSE)));\n"
        "    float lMax = max(lM, max(max(lNW,lNE), max(lSW,lSE)));\n"
        "    vec2 dir = vec2(-((lNW+lNE)-(lSW+lSE)), ((lNW+lSW)-(lNE+lSE)));\n"
        "    float reduce = max((lNW+lNE+lSW+lSE)*0.03125, 0.0078125);\n"
        "    float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);\n"
        "    dir = clamp(dir*rcp, -8.0, 8.0) * uInvRes;\n"
        "    vec3 rA = 0.5*(texture(uTex, vUV+dir*(1.0/3.0-0.5)).rgb + texture(uTex, vUV+dir*(2.0/3.0-0.5)).rgb);\n"
        "    vec3 rB = rA*0.5 + 0.25*(texture(uTex, vUV-dir*0.5).rgb + texture(uTex, vUV+dir*0.5).rgb);\n"
        "    float lB = luma(rB);\n"
        "    FragColor = vec4((lB < lMin || lB > lMax) ? rA : rB, 1.0);\n"
        "}\n";

    inline void makeColorTex(GLuint& t, int w, int h)
    {
        if (!t) glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        // RGBA16F: HDR — lighting can exceed 1.0, tonemapped at composite.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

// Offscreen scene target + bloom chain + composite pass.
class PostFX {
private:
    GLuint sceneFbo = 0, sceneTex = 0, depthRbo = 0;
    GLuint brightFbo = 0, brightTex = 0;
    GLuint blurFbo[2] = {0, 0}, blurTex[2] = {0, 0};
    GLuint ldrFbo = 0, ldrTex = 0;
    GLuint progBright = 0, progBlur = 0, progComposite = 0, progFxaa = 0, vao = 0;
    GLint uBrightScene = -1, uBlurTex = -1, uBlurDir = -1, uCompScene = -1, uCompBloom = -1;
    GLint uFxaaTex = -1, uFxaaInvRes = -1;
    int w = 0, h = 0, bw = 0, bh = 0;

    void alloc(int width, int height)
    {
        w = width; h = height;
        bw = width / 2; bh = height / 2;

        makeColorTex(sceneTex, w, h);
        if (!depthRbo) glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        if (!sceneFbo) glGenFramebuffers(1, &sceneFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

        makeColorTex(brightTex, bw, bh);
        if (!brightFbo) glGenFramebuffers(1, &brightFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brightTex, 0);

        for (int i = 0; i < 2; ++i)
        {
            makeColorTex(blurTex[i], bw, bh);
            if (!blurFbo[i]) glGenFramebuffers(1, &blurFbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, blurFbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex[i], 0);
        }

        // LDR target (full res) for the composite output that FXAA then reads.
        if (!ldrTex) glGenTextures(1, &ldrTex);
        glBindTexture(GL_TEXTURE_2D, ldrTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!ldrFbo) glGenFramebuffers(1, &ldrFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, ldrFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ldrTex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "PostFX FBO incomplete" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void fullscreen() { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, 3); glBindVertexArray(0); }

public:
    void init(int width, int height)
    {
        progBright = tools::linkProgram(kFsVert, kBrightFrag);
        progBlur = tools::linkProgram(kFsVert, kBlurFrag);
        progComposite = tools::linkProgram(kFsVert, kCompositeFrag);
        progFxaa = tools::linkProgram(kFsVert, kFxaaFrag);
        uBrightScene = glGetUniformLocation(progBright, "uScene");
        uBlurTex = glGetUniformLocation(progBlur, "uTex");
        uBlurDir = glGetUniformLocation(progBlur, "uDir");
        uCompScene = glGetUniformLocation(progComposite, "uScene");
        uCompBloom = glGetUniformLocation(progComposite, "uBloom");
        uFxaaTex = glGetUniformLocation(progFxaa, "uTex");
        uFxaaInvRes = glGetUniformLocation(progFxaa, "uInvRes");
        glGenVertexArrays(1, &vao);
        alloc(width, height);
    }

    void resize(int width, int height)
    {
        if (width == w && height == h) return;
        alloc(width, height);
    }

    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo); glViewport(0, 0, w, h); }

    // Bright-pass, blur, composite to the default framebuffer.
    void draw(int screenW, int screenH)
    {
        glDisable(GL_DEPTH_TEST);

        // Bright-pass (half res).
        glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
        glViewport(0, 0, bw, bh);
        glUseProgram(progBright);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);
        glUniform1i(uBrightScene, 0);
        fullscreen();

        // Ping-pong Gaussian blur.
        glUseProgram(progBlur);
        glUniform1i(uBlurTex, 0);
        GLuint src = brightTex;
        for (int i = 0; i < 6; ++i)
        {
            int dst = i % 2;
            glBindFramebuffer(GL_FRAMEBUFFER, blurFbo[dst]);
            glViewport(0, 0, bw, bh);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, src);
            if (i % 2 == 0) glUniform2f(uBlurDir, 1.0f / bw, 0.0f);
            else            glUniform2f(uBlurDir, 0.0f, 1.0f / bh);
            fullscreen();
            src = blurTex[dst];
        }

        // Composite (HDR + bloom + tonemap) into the LDR target.
        glBindFramebuffer(GL_FRAMEBUFFER, ldrFbo);
        glViewport(0, 0, w, h);
        glUseProgram(progComposite);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);
        glUniform1i(uCompScene, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, src);
        glUniform1i(uCompBloom, 1);
        glActiveTexture(GL_TEXTURE0);
        fullscreen();

        // FXAA to the screen.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenW, screenH);
        glUseProgram(progFxaa);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ldrTex);
        glUniform1i(uFxaaTex, 0);
        glUniform2f(uFxaaInvRes, 1.0f / w, 1.0f / h);
        fullscreen();

        glEnable(GL_DEPTH_TEST);
    }

    void free()
    {
        GLuint fbos[] = {sceneFbo, brightFbo, blurFbo[0], blurFbo[1], ldrFbo};
        glDeleteFramebuffers(5, fbos);
        GLuint texs[] = {sceneTex, brightTex, blurTex[0], blurTex[1], ldrTex};
        glDeleteTextures(5, texs);
        if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        glDeleteProgram(progBright);
        glDeleteProgram(progBlur);
        glDeleteProgram(progComposite);
        glDeleteProgram(progFxaa);
    }
};

} // namespace smallgine
