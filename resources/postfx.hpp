#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../core/constants.hpp"
#include <glm/glm.hpp>
#include <iostream>

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
    GLuint sceneFbo = 0, sceneTex = 0, depthTex = 0;
    GLuint brightFbo = 0, brightTex = 0;
    GLuint blurFbo[2] = {0, 0}, blurTex[2] = {0, 0};
    GLuint dofFbo[2] = {0, 0}, dofTex[2] = {0, 0}; // half-res blurred scene (depth of field)
    GLuint ldrFbo = 0, ldrTex = 0;
    GLuint progBright = 0, progBlur = 0, progComposite = 0, progFxaa = 0, vao = 0;
    GLint uBrightScene = -1, uBlurTex = -1, uBlurDir = -1, uCompScene = -1, uCompBloom = -1;
    GLint uCompDof = -1, uCompDepth = -1, uCompNear = -1, uCompFar = -1, uCompTime = -1;
    GLint uCompInvVP = -1, uCompPrevVP = -1;
    GLint uFxaaTex = -1, uFxaaInvRes = -1;
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
        if (!sceneFbo) glGenFramebuffers(1, &sceneFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

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

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "PostFX FBO incomplete" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void fullscreen() { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, 3); glBindVertexArray(0); }

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
    void draw(int screenW, int screenH, float nearP, float farP, float time,
              const glm::mat4& invVP, const glm::mat4& prevVP)
    {
        glDisable(GL_DEPTH_TEST);

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
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthTex);  glUniform1i(uCompDepth, 3);
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
        GLuint fbos[] = {sceneFbo, brightFbo, blurFbo[0], blurFbo[1], dofFbo[0], dofFbo[1], ldrFbo};
        glDeleteFramebuffers(7, fbos);
        GLuint texs[] = {sceneTex, brightTex, blurTex[0], blurTex[1], dofTex[0], dofTex[1], ldrTex, depthTex};
        glDeleteTextures(8, texs);
        if (vao) glDeleteVertexArrays(1, &vao);
        glDeleteProgram(progBright);
        glDeleteProgram(progBlur);
        glDeleteProgram(progComposite);
        glDeleteProgram(progFxaa);
    }
};

} // namespace smallgine
