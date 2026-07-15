#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../core/node.hpp"
#include "../core/scene.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace smallgine {

// Omnidirectional shadow map for one point light: linear distance in an R16F
// cubemap, rendered from the light's position across 6 faces.
class ShadowCube {
private:
    GLuint fbo = 0, cube = 0, depthRbo = 0, prog = 0;
    GLint uVP = -1, uModel = -1, uLightPos = -1, uFar = -1;
    int size = 512;
    float farP = 25.0f;

    void drawNode(const Node& n, const glm::mat4& parent)
    {
        glm::mat4 global = parent * n.localMatrix();
        if (n.mesh)
        {
            glUniformMatrix4fv(uModel, 1, GL_FALSE, &global[0][0]);
            n.mesh->draw();
        }
        for (const Node& c : n.Children) drawNode(c, global);
    }

public:
    float farPlane() const { return farP; }

    void init(int s = 512)
    {
        size = s;
        prog = tools::linkProgramFiles("assets/shaders/pointdepth.vert", "assets/shaders/pointdepth.frag");
        uVP = glGetUniformLocation(prog, "uVP");
        uModel = glGetUniformLocation(prog, "uModel");
        uLightPos = glGetUniformLocation(prog, "uLightPos");
        uFar = glGetUniformLocation(prog, "uFar");

        glGenTextures(1, &cube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
        for (int i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R16F, size, size, 0, GL_RED, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Render scene distance into all 6 faces from the light position.
    void render(const Scene& scene, const glm::vec3& lightPos, int restoreW, int restoreH)
    {
        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, farP);
        glm::vec3 dirs[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
        glm::vec3 ups[6]  = { {0,-1,0},{0,-1,0}, {0,0,1}, {0,0,-1},{0,-1,0},{0,-1,0} };

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, size, size);
        glUseProgram(prog);
        glUniform3fv(uLightPos, 1, &lightPos[0]);
        glUniform1f(uFar, farP);
        for (int f = 0; f < 6; ++f)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cube, 0);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // far = 1.0 (unshadowed)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 vp = proj * glm::lookAt(lightPos, lightPos + dirs[f], ups[f]);
            glUniformMatrix4fv(uVP, 1, GL_FALSE, &vp[0][0]);
            drawNode(scene.MainNode, glm::mat4(1.0f));
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, restoreW, restoreH);
    }

    GLuint texture() const { return cube; }

    void free()
    {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (cube) glDeleteTextures(1, &cube);
        if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
