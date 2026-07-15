#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../tools/helpers.hpp"
#include "../core/constants.hpp"
#include "../core/light.hpp"
#include "csm.hpp"
#include "pointshadow.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <iostream>

namespace smallgine {

// Contained deferred renderer for OPAQUE geometry: a G-buffer (albedo/spec,
// world normal/shininess, depth) plus a fullscreen lighting pass. Forward
// transparency / instances / particles are composited on top afterwards, so
// nothing in the forward path is removed.
class Deferred {
private:
    GLuint gFbo = 0, gAlbedo = 0, gNormal = 0, gDepth = 0;
    GLuint gProg = 0, lProg = 0, vao = 0;
    // Geometry program uniforms (reused via tools::drawItem).
    GLint gMVP = -1, gModel = -1, gColor = -1, gMetallic = -1, gRoughness = -1, gTexU = -1;
    // Lighting program uniforms.
    GLint lAlbedo = -1, lNormal = -1, lDepth = -1, lInvVP = -1, lViewPos = -1;
    GLint lNumLights = -1, lType = -1, lPos = -1, lCol = -1, lInten = -1;
    GLint lCSM = -1, lCSMMat = -1, lCSMSplit = -1;
    GLint lPointShadow = -1, lHasPoint = -1, lPointPos = -1, lPointFar = -1, lEnv = -1;
    int w = 0, h = 0;

    void alloc(int width, int height)
    {
        w = width; h = height;
        auto mk = [](GLuint& t, int ww, int hh) {
            if (!t) glGenTextures(1, &t);
            glBindTexture(GL_TEXTURE_2D, t);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, ww, hh, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
        mk(gAlbedo, w, h);
        mk(gNormal, w, h);
        if (!gDepth) glGenTextures(1, &gDepth);
        glBindTexture(GL_TEXTURE_2D, gDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (!gFbo) glGenFramebuffers(1, &gFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedo, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);
        GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Deferred G-buffer incomplete" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

public:
    void init(int width, int height)
    {
        gProg = tools::linkProgramFiles("assets/shaders/gbuffer.vert", "assets/shaders/gbuffer.frag");
        lProg = tools::linkProgramFiles("assets/shaders/fullscreen.vert", "assets/shaders/deferred.frag");
        gMVP = glGetUniformLocation(gProg, "uMVP");
        gModel = glGetUniformLocation(gProg, "uModel");
        gColor = glGetUniformLocation(gProg, "uColor");
        gMetallic = glGetUniformLocation(gProg, "uMetallic");
        gRoughness = glGetUniformLocation(gProg, "uRoughness");
        gTexU = glGetUniformLocation(gProg, "uTex");
        lAlbedo = glGetUniformLocation(lProg, "uAlbedo");
        lNormal = glGetUniformLocation(lProg, "uNormalTex");
        lDepth = glGetUniformLocation(lProg, "uDepth");
        lInvVP = glGetUniformLocation(lProg, "uInvVP");
        lViewPos = glGetUniformLocation(lProg, "uViewPos");
        lNumLights = glGetUniformLocation(lProg, "uNumLights");
        lType = glGetUniformLocation(lProg, "uLightType");
        lPos = glGetUniformLocation(lProg, "uLightPos");
        lCol = glGetUniformLocation(lProg, "uLightColor");
        lInten = glGetUniformLocation(lProg, "uLightIntensity");
        lCSM = glGetUniformLocation(lProg, "uCSM");
        lCSMMat = glGetUniformLocation(lProg, "uCSMMat");
        lCSMSplit = glGetUniformLocation(lProg, "uCSMSplit");
        lPointShadow = glGetUniformLocation(lProg, "uPointShadow");
        lHasPoint = glGetUniformLocation(lProg, "uHasPointShadow");
        lPointPos = glGetUniformLocation(lProg, "uPointLightPos");
        lPointFar = glGetUniformLocation(lProg, "uPointFar");
        lEnv = glGetUniformLocation(lProg, "uEnv");
        glGenVertexArrays(1, &vao);
        alloc(width, height);
    }

    void resize(int width, int height) { if (width != w || height != h) alloc(width, height); }

    // Bind the G-buffer and return the uniform set for tools::drawItem to fill.
    tools::DrawUniforms beginGeometry()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(gProg);
        glUniform1i(gTexU, 0);
        tools::DrawUniforms u;
        u.mvp = gMVP; u.model = gModel; u.color = gColor;
        u.metallic = gMetallic; u.roughness = gRoughness;
        // alpha / normalMap / parallax / terrain unused in the G-buffer (stay -1).
        return u;
    }

    // Fullscreen lighting into the currently-bound framebuffer.
    void lighting(const glm::mat4& invVP, const glm::vec3& viewPos,
                  const std::vector<Light>& lights, const CascadedShadow& csm,
                  const ShadowCube& pointShadow, bool hasPoint, const glm::vec3& pointPos,
                  GLuint envCube)
    {
        int n = (int)lights.size(); if (n > k::MaxLights) n = k::MaxLights;
        int types[k::MaxLights]; float pos[k::MaxLights*3], col[k::MaxLights*3], inten[k::MaxLights];
        for (int i = 0; i < n; ++i)
        {
            types[i] = lights[i].type;
            pos[i*3]=lights[i].position.x; pos[i*3+1]=lights[i].position.y; pos[i*3+2]=lights[i].position.z;
            col[i*3]=lights[i].color.x;    col[i*3+1]=lights[i].color.y;    col[i*3+2]=lights[i].color.z;
            inten[i]=lights[i].intensity;
        }
        glUseProgram(lProg);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gAlbedo); glUniform1i(lAlbedo, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal); glUniform1i(lNormal, 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gDepth);  glUniform1i(lDepth, 2);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D_ARRAY, csm.texture()); glUniform1i(lCSM, 5);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadow.texture()); glUniform1i(lPointShadow, 4);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, envCube); glUniform1i(lEnv, 6);
        glActiveTexture(GL_TEXTURE0);
        glUniformMatrix4fv(lInvVP, 1, GL_FALSE, &invVP[0][0]);
        glUniform3fv(lViewPos, 1, &viewPos[0]);
        glUniform1i(lNumLights, n);
        glUniform1iv(lType, n, types);
        glUniform3fv(lPos, n, pos);
        glUniform3fv(lCol, n, col);
        glUniform1fv(lInten, n, inten);
        glUniformMatrix4fv(lCSMMat, CascadedShadow::N, GL_FALSE, &csm.matrices()[0][0][0]);
        glUniform1fv(lCSMSplit, CascadedShadow::N, csm.splitDepths());
        glUniform1i(lHasPoint, hasPoint ? 1 : 0);
        glUniform3fv(lPointPos, 1, &pointPos[0]);
        glUniform1f(lPointFar, pointShadow.farPlane());
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    // Copy G-buffer depth into another framebuffer so forward overlay depth-tests.
    void blitDepthTo(GLuint dstFbo)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    void free()
    {
        if (gFbo) glDeleteFramebuffers(1, &gFbo);
        GLuint t[3] = {gAlbedo, gNormal, gDepth}; glDeleteTextures(3, t);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (gProg) glDeleteProgram(gProg);
        if (lProg) glDeleteProgram(lProg);
    }
};

} // namespace smallgine
