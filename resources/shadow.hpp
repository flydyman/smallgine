#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>

namespace smallgine {

namespace {
    const char* kDepthVert =
        "#version 310 es\n"
        "layout(location = 0) in vec3 aPos;\n"
        "uniform mat4 uLightMVP;\n"
        "void main() { gl_Position = uLightMVP * vec4(aPos, 1.0); }\n";

    const char* kDepthFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "void main() {}\n";
}

// Directional-light shadow map: renders scene depth from the light's view.
class ShadowMap {
private:
    GLuint fbo = 0, depthTex = 0, prog = 0;
    int size = 1024;

public:
    GLint uLightMVP = -1;

    void init(int s = 1024)
    {
        size = s;
        prog = tools::linkProgram(kDepthVert, kDepthFrag);
        uLightMVP = glGetUniformLocation(prog, "uLightMVP");

        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "Shadow FBO incomplete" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Ortho light-space matrix looking at the origin from the light direction.
    glm::mat4 lightSpace(const glm::vec3& dirToLight) const
    {
        glm::vec3 d = glm::normalize(dirToLight);
        glm::vec3 pos = d * 8.0f;
        glm::mat4 proj = glm::ortho(-4.0f, 4.0f, -4.0f, 4.0f, 0.1f, 20.0f);
        glm::vec3 up = (std::abs(d.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f), up);
        return proj * view;
    }

    void begin()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, size, size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);
        glCullFace(GL_FRONT); // cast from back faces => less acne / peter-panning
    }

    void end(int w, int h)
    {
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
    }

    GLuint texture() const { return depthTex; }

    void free()
    {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (depthTex) glDeleteTextures(1, &depthTex);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
