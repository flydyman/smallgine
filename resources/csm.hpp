#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../tools/helpers.hpp"
#include "../core/scene.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <iostream>

namespace smallgine {

// Cascaded shadow maps for the directional light: N depth slices covering the
// camera frustum near-to-far, rendered into a 2D depth-texture array.
class CascadedShadow {
public:
    static const int N = 3;

private:
    GLuint fbo = 0, depthArray = 0, prog = 0;
    GLint uLightMVP = -1;
    int size = 1024;
    std::array<glm::mat4, N> mats;
    std::array<float, N> splits;   // far view-distance of each cascade

public:
    void init(int s = 1024)
    {
        size = s;
        prog = tools::linkProgramFiles("assets/shaders/depth.vert", "assets/shaders/empty.frag");
        uLightMVP = glGetUniformLocation(prog, "uLightMVP");

        glGenTextures(1, &depthArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, size, size, N, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Fit each cascade's ortho box to a slice of the camera frustum.
    void update(const glm::mat4& camView, float fov, float aspect, const glm::vec3& dirToLight)
    {
        const float ranges[N + 1] = {k::PerspectiveNear, 7.0f, 18.0f, 55.0f};
        glm::vec3 L = glm::normalize(dirToLight);
        for (int i = 0; i < N; ++i)
        {
            glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, ranges[i], ranges[i + 1]);
            glm::mat4 inv = glm::inverse(proj * camView);
            glm::vec3 center(0.0f);
            glm::vec3 corners[8];
            int k = 0;
            for (int x = -1; x <= 1; x += 2)
                for (int y = -1; y <= 1; y += 2)
                    for (int z = -1; z <= 1; z += 2)
                    {
                        glm::vec4 c = inv * glm::vec4((float)x, (float)y, (float)z, 1.0f);
                        corners[k] = glm::vec3(c) / c.w;
                        center += corners[k];
                        ++k;
                    }
            center /= 8.0f;
            float radius = 0.0f;
            for (int j = 0; j < 8; ++j) radius = std::max(radius, glm::length(corners[j] - center));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 up = (std::abs(L.y) > 0.99f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::mat4 lview = glm::lookAt(center + L * radius, center, up);
            glm::mat4 lproj = glm::ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius);
            mats[i] = lproj * lview;
            splits[i] = ranges[i + 1];
        }
    }

    // Render scene depth into each cascade layer (front-cull to reduce acne).
    void render(const Scene& scene, int restoreW, int restoreH)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, size, size);
        glUseProgram(prog);
        glCullFace(GL_FRONT);
        for (int i = 0; i < N; ++i)
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArray, 0, i);
            glClear(GL_DEPTH_BUFFER_BIT);
            tools::DrawSceneDepth(scene, uLightMVP, mats[i]);
        }
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, restoreW, restoreH);
    }

    GLuint texture() const { return depthArray; }
    const glm::mat4* matrices() const { return mats.data(); }
    const float* splitDepths() const { return splits.data(); }

    void free()
    {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (depthArray) glDeleteTextures(1, &depthArray);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
