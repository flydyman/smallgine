#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../core/constants.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace smallgine {

namespace {

    // Bright-pass: keep only luminance above a threshold.

    // Separable Gaussian blur (direction in texels).

    // Composite: scene + bloom, then gamma / vignette / saturation.


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
    GLuint sceneFbo = 0, sceneTex = 0, depthTex = 0, normalTex = 0;
    GLuint brightFbo = 0, brightTex = 0;
    GLuint blurFbo[2] = {0, 0}, blurTex[2] = {0, 0};
    GLuint dofFbo[2] = {0, 0}, dofTex[2] = {0, 0}; // half-res blurred scene (depth of field)
    GLuint ldrFbo = 0, ldrTex = 0;
    GLuint ssaoFbo = 0, ssaoTex = 0, ssaoBlurFbo = 0, ssaoBlurTex = 0, noiseTex = 0;
    GLuint ssrFbo = 0, ssrTex = 0;
    GLuint shaftFbo = 0, shaftTex = 0, lutTex = 0;
    GLuint progShaft = 0;
    GLint uShaftDepth = -1, uShaftSunUV = -1, uShaftVisible = -1;
    GLint uCompShaft = -1, uCompLut = -1, uCompFocus = -1, uCompSunColor = -1, uCompCinematic = -1;
    GLint uCompBloomStrength = -1, uCompDofStrength = -1, uCompMotionStrength = -1;
    GLuint progBright = 0, progBlur = 0, progComposite = 0, progFxaa = 0, vao = 0;
    GLuint progSSAO = 0, progSSAOBlur = 0, progSSR = 0;
    GLint uBrightScene = -1, uBlurTex = -1, uBlurDir = -1, uCompScene = -1, uCompBloom = -1;
    GLint uCompDof = -1, uCompDepth = -1, uCompNear = -1, uCompFar = -1, uCompTime = -1;
    GLint uCompInvVP = -1, uCompPrevVP = -1, uCompAO = -1, uCompSSR = -1;
    GLint uFxaaTex = -1, uFxaaInvRes = -1;
    GLint uSsaoDepth = -1, uSsaoNormal = -1, uSsaoNoise = -1, uSsaoVP = -1, uSsaoInvVP = -1;
    GLint uSsaoCam = -1, uSsaoNoiseScale = -1, uSsaoRadius = -1, uSsaoSamples = -1;
    GLint uSbTex = -1, uSbTexel = -1, uSbDepth = -1;
    GLint uSsrScene = -1, uSsrDepth = -1, uSsrNormal = -1, uSsrVP = -1, uSsrInvVP = -1, uSsrCam = -1;
    std::vector<glm::vec3> kernel;
    bool ssaoOn = true, ssrOn = true;
    int w = 0, h = 0, bw = 0, bh = 0;

    void alloc(int width, int height)
    {
        w = width; h = height;
        bw = width / 2; bh = height / 2;

        makeColorTex(sceneTex, w, h);
        // Depth as a sampleable texture (fog / DoF / motion blur read it).
        if (!depthTex) glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // World-space normal target (G-buffer attachment for SSAO / SSR).
        makeColorTex(normalTex, w, h);
        if (!sceneFbo) glGenFramebuffers(1, &sceneFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        { GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}; glDrawBuffers(2, bufs); }

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

            makeColorTex(dofTex[i], bw, bh);
            if (!dofFbo[i]) glGenFramebuffers(1, &dofFbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, dofFbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dofTex[i], 0);
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

        // SSAO (raw + blurred, half res) and SSR (half res HDR).
        makeColorTex(ssaoTex, bw, bh);
        if (!ssaoFbo) glGenFramebuffers(1, &ssaoFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTex, 0);
        { GLenum b = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &b); }
        makeColorTex(ssaoBlurTex, bw, bh);
        if (!ssaoBlurFbo) glGenFramebuffers(1, &ssaoBlurFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTex, 0);
        makeColorTex(ssrTex, bw, bh);
        if (!ssrFbo) glGenFramebuffers(1, &ssrFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, ssrFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssrTex, 0);

        makeColorTex(shaftTex, bw, bh);
        if (!shaftFbo) glGenFramebuffers(1, &shaftFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, shaftFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shaftTex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "PostFX FBO incomplete" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void fullscreen() { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, 3); glBindVertexArray(0); }

    // Bake a 16^3 color grade into a 256x16 strip (warm highlights / teal shadows).
    void buildLut()
    {
        const int S = 16;
        std::vector<unsigned char> px(256 * 16 * 4);
        for (int b = 0; b < S; ++b)
            for (int g = 0; g < S; ++g)
                for (int r = 0; r < S; ++r)
                {
                    glm::vec3 c(r / 15.0f, g / 15.0f, b / 15.0f);
                    c = glm::pow(c, glm::vec3(0.95f));                 // slight shadow lift
                    float l = glm::dot(c, glm::vec3(0.299f, 0.587f, 0.114f));
                    glm::vec3 shadow(0.00f, 0.02f, 0.06f), high(0.06f, 0.03f, 0.00f);
                    float t = glm::smoothstep(0.2f, 0.8f, l);
                    c += glm::mix(shadow, high, t);                    // split tone
                    c = (c - 0.5f) * 1.08f + 0.5f;                     // contrast
                    c = glm::mix(glm::vec3(l), c, 1.12f);              // saturation
                    c = glm::clamp(c, 0.0f, 1.0f);
                    int x = b * S + r, y = g;
                    int i = (y * 256 + x) * 4;
                    px[i+0] = (unsigned char)(c.r * 255.0f);
                    px[i+1] = (unsigned char)(c.g * 255.0f);
                    px[i+2] = (unsigned char)(c.b * 255.0f);
                    px[i+3] = 255;
                }
        glGenTextures(1, &lutTex);
        glBindTexture(GL_TEXTURE_2D, lutTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

public:
    void init(int width, int height)
    {
        progBright = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/bright.frag");
        progBlur = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/blur.frag");
        progComposite = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/composite.frag");
        progFxaa = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/fxaa.frag");
        uBrightScene = glGetUniformLocation(progBright, "uScene");
        uBlurTex = glGetUniformLocation(progBlur, "uTex");
        uBlurDir = glGetUniformLocation(progBlur, "uDir");
        uCompScene = glGetUniformLocation(progComposite, "uScene");
        uCompBloom = glGetUniformLocation(progComposite, "uBloom");
        uCompDof = glGetUniformLocation(progComposite, "uDof");
        uCompDepth = glGetUniformLocation(progComposite, "uDepth");
        uCompNear = glGetUniformLocation(progComposite, "uNear");
        uCompFar = glGetUniformLocation(progComposite, "uFar");
        uCompTime = glGetUniformLocation(progComposite, "uTime");
        uCompInvVP = glGetUniformLocation(progComposite, "uInvVP");
        uCompPrevVP = glGetUniformLocation(progComposite, "uPrevVP");
        uCompAO = glGetUniformLocation(progComposite, "uAO");
        uCompSSR = glGetUniformLocation(progComposite, "uSSR");
        uCompShaft = glGetUniformLocation(progComposite, "uShaft");
        uCompLut = glGetUniformLocation(progComposite, "uLut");
        uCompFocus = glGetUniformLocation(progComposite, "uFocusDist");
        uCompCinematic = glGetUniformLocation(progComposite, "uCinematic");
        uCompBloomStrength = glGetUniformLocation(progComposite, "uBloomStrength");
        uCompDofStrength = glGetUniformLocation(progComposite, "uDofStrength");
        uCompMotionStrength = glGetUniformLocation(progComposite, "uMotionStrength");
        uCompSunColor = glGetUniformLocation(progComposite, "uSunColor");
        uFxaaTex = glGetUniformLocation(progFxaa, "uTex");
        uFxaaInvRes = glGetUniformLocation(progFxaa, "uInvRes");

        progSSAO = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/ssao.frag");
        progSSAOBlur = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/ssao_blur.frag");
        progSSR = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/ssr.frag");
        progShaft = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/lightshaft.frag");
        uShaftDepth = glGetUniformLocation(progShaft, "uDepth");
        uShaftSunUV = glGetUniformLocation(progShaft, "uSunUV");
        uShaftVisible = glGetUniformLocation(progShaft, "uSunVisible");
        buildLut();
        uSsaoDepth = glGetUniformLocation(progSSAO, "uDepth");
        uSsaoNormal = glGetUniformLocation(progSSAO, "uNormal");
        uSsaoNoise = glGetUniformLocation(progSSAO, "uNoise");
        uSsaoVP = glGetUniformLocation(progSSAO, "uViewProj");
        uSsaoInvVP = glGetUniformLocation(progSSAO, "uInvVP");
        uSsaoCam = glGetUniformLocation(progSSAO, "uCamPos");
        uSsaoNoiseScale = glGetUniformLocation(progSSAO, "uNoiseScale");
        uSsaoRadius = glGetUniformLocation(progSSAO, "uRadius");
        uSsaoSamples = glGetUniformLocation(progSSAO, "uSamples");
        uSbTex = glGetUniformLocation(progSSAOBlur, "uTex");
        uSbTexel = glGetUniformLocation(progSSAOBlur, "uTexel");
        uSbDepth = glGetUniformLocation(progSSAOBlur, "uDepth");
        uSsrScene = glGetUniformLocation(progSSR, "uScene");
        uSsrDepth = glGetUniformLocation(progSSR, "uDepth");
        uSsrNormal = glGetUniformLocation(progSSR, "uNormal");
        uSsrVP = glGetUniformLocation(progSSR, "uViewProj");
        uSsrInvVP = glGetUniformLocation(progSSR, "uInvVP");
        uSsrCam = glGetUniformLocation(progSSR, "uCamPos");

        // Hemisphere sample kernel, biased toward the origin.
        auto rnd = []() { return (float)(std::rand() % 2000) / 1000.0f - 1.0f; };
        for (int i = 0; i < 16; ++i)
        {
            glm::vec3 s(rnd(), rnd(), (float)(std::rand() % 1000) / 1000.0f); // z >= 0 (hemisphere)
            s = glm::normalize(s);
            float scale = (float)i / 16.0f;
            s *= 0.1f + 0.9f * scale * scale; // cluster near origin
            kernel.push_back(s);
        }

        // 4x4 random rotation noise.
        std::vector<float> noise(4 * 4 * 3);
        for (int i = 0; i < 16; ++i) { noise[i*3+0] = rnd(); noise[i*3+1] = rnd(); noise[i*3+2] = 0.0f; }
        glGenTextures(1, &noiseTex);
        glBindTexture(GL_TEXTURE_2D, noiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenVertexArrays(1, &vao);
        alloc(width, height);
    }

    void toggleSSAO() { ssaoOn = !ssaoOn; }
    void toggleSSR() { ssrOn = !ssrOn; }
    bool ssaoEnabled() const { return ssaoOn; }
    bool ssrEnabled() const { return ssrOn; }

    void resize(int width, int height)
    {
        if (width == w && height == h) return;
        alloc(width, height);
    }

    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo); glViewport(0, 0, w, h); }
    GLuint sceneFramebuffer() const { return sceneFbo; }

    // Bright-pass, blur, composite to the default framebuffer.
    void draw(int screenW, int screenH, float nearP, float farP, float time,
              const glm::mat4& invVP, const glm::mat4& prevVP,
              const glm::mat4& viewProj, const glm::vec3& camPos,
              float focusDist, const glm::vec2& sunUV, float sunVisible, const glm::vec3& sunColor,
              float cinematic = 1.0f)
    {
        glDisable(GL_DEPTH_TEST);

        // SSAO: raw occlusion then box blur (half res). Skipped => white (no AO).
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFbo);
        glViewport(0, 0, bw, bh);
        if (ssaoOn)
        {
            glUseProgram(progSSAO);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, depthTex);  glUniform1i(uSsaoDepth, 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, normalTex); glUniform1i(uSsaoNormal, 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);  glUniform1i(uSsaoNoise, 2);
            glUniformMatrix4fv(uSsaoVP, 1, GL_FALSE, &viewProj[0][0]);
            glUniformMatrix4fv(uSsaoInvVP, 1, GL_FALSE, &invVP[0][0]);
            glUniform3fv(uSsaoCam, 1, &camPos[0]);
            glUniform2f(uSsaoNoiseScale, (float)bw / 4.0f, (float)bh / 4.0f);
            glUniform1f(uSsaoRadius, 0.6f);
            glUniform3fv(uSsaoSamples, (GLsizei)kernel.size(), &kernel[0][0]);
            fullscreen();

            glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFbo);
            glViewport(0, 0, bw, bh);
            glUseProgram(progSSAOBlur);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoTex);  glUniform1i(uSbTex, 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthTex); glUniform1i(uSbDepth, 1);
            glUniform2f(uSbTexel, 1.0f / bw, 1.0f / bh);
            fullscreen();
        }
        else { glClearColor(1.0f, 1.0f, 1.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
               glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFbo); glViewport(0, 0, bw, bh); glClear(GL_COLOR_BUFFER_BIT); }

        // SSR: march reflections in the G-buffer. Skipped => black (no reflection).
        glBindFramebuffer(GL_FRAMEBUFFER, ssrFbo);
        glViewport(0, 0, bw, bh);
        if (ssrOn)
        {
            glUseProgram(progSSR);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);  glUniform1i(uSsrScene, 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthTex);  glUniform1i(uSsrDepth, 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, normalTex); glUniform1i(uSsrNormal, 2);
            glUniformMatrix4fv(uSsrVP, 1, GL_FALSE, &viewProj[0][0]);
            glUniformMatrix4fv(uSsrInvVP, 1, GL_FALSE, &invVP[0][0]);
            glUniform3fv(uSsrCam, 1, &camPos[0]);
            fullscreen();
        }
        else { glClearColor(0.0f, 0.0f, 0.0f, 0.0f); glClear(GL_COLOR_BUFFER_BIT); }

        // Volumetric light shafts: radial march of the sky mask toward the sun.
        glBindFramebuffer(GL_FRAMEBUFFER, shaftFbo);
        glViewport(0, 0, bw, bh);
        glUseProgram(progShaft);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, depthTex); glUniform1i(uShaftDepth, 0);
        glUniform2f(uShaftSunUV, sunUV.x, sunUV.y);
        glUniform1f(uShaftVisible, sunVisible);
        fullscreen();

        // Bright-pass (half res).
        glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
        glViewport(0, 0, bw, bh);
        glUseProgram(progBright);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);
        glUniform1i(uBrightScene, 0);
        fullscreen();

        // Ping-pong Gaussian blur (bloom).
        glUseProgram(progBlur);
        glUniform1i(uBlurTex, 0);
        GLuint src = brightTex;
        for (int i = 0; i < k::BloomBlurPasses; ++i)
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
        GLuint bloomTex = src;

        // Blur the full scene (half res) for depth of field.
        GLuint dsrc = sceneTex;
        for (int i = 0; i < 4; ++i)
        {
            int dst = i % 2;
            glBindFramebuffer(GL_FRAMEBUFFER, dofFbo[dst]);
            glViewport(0, 0, bw, bh);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, dsrc);
            if (i % 2 == 0) glUniform2f(uBlurDir, 1.0f / bw, 0.0f);
            else            glUniform2f(uBlurDir, 0.0f, 1.0f / bh);
            fullscreen();
            dsrc = dofTex[dst];
        }

        // Composite: motion blur + DoF + fog + bloom + tonemap + CA + grain.
        glBindFramebuffer(GL_FRAMEBUFFER, ldrFbo);
        glViewport(0, 0, w, h);
        glUseProgram(progComposite);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneTex);  glUniform1i(uCompScene, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, bloomTex);  glUniform1i(uCompBloom, 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, dsrc);      glUniform1i(uCompDof, 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthTex);      glUniform1i(uCompDepth, 3);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, ssaoBlurTex);  glUniform1i(uCompAO, 4);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, ssrTex);       glUniform1i(uCompSSR, 5);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, shaftTex);     glUniform1i(uCompShaft, 6);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, lutTex);       glUniform1i(uCompLut, 7);
        glUniform1f(uCompFocus, focusDist);
        glUniform1f(uCompCinematic, cinematic);
        glUniform1f(uCompBloomStrength, k::BloomStrength);
        glUniform1f(uCompDofStrength, k::DofStrength);
        glUniform1f(uCompMotionStrength, k::MotionBlurStrength);
        glUniform3fv(uCompSunColor, 1, &sunColor[0]);
        glUniform1f(uCompNear, nearP);
        glUniform1f(uCompFar, farP);
        glUniform1f(uCompTime, time);
        glUniformMatrix4fv(uCompInvVP, 1, GL_FALSE, &invVP[0][0]);
        glUniformMatrix4fv(uCompPrevVP, 1, GL_FALSE, &prevVP[0][0]);
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
        GLuint fbos[] = {sceneFbo, brightFbo, blurFbo[0], blurFbo[1], dofFbo[0], dofFbo[1], ldrFbo,
                         ssaoFbo, ssaoBlurFbo, ssrFbo, shaftFbo};
        glDeleteFramebuffers(11, fbos);
        GLuint texs[] = {sceneTex, brightTex, blurTex[0], blurTex[1], dofTex[0], dofTex[1], ldrTex, depthTex,
                         normalTex, ssaoTex, ssaoBlurTex, ssrTex, noiseTex, shaftTex, lutTex};
        glDeleteTextures(15, texs);
        if (vao) glDeleteVertexArrays(1, &vao);
        glDeleteProgram(progBright);
        glDeleteProgram(progBlur);
        glDeleteProgram(progComposite);
        glDeleteProgram(progFxaa);
        glDeleteProgram(progSSAO);
        glDeleteProgram(progSSAOBlur);
        glDeleteProgram(progSSR);
        glDeleteProgram(progShaft);
    }
};

} // namespace smallgine
