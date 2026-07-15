#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace smallgine {

namespace {


    // Instanced depth-only shader for the shadow pass.
}

// Draws many copies of one mesh in a single call via instanced arrays.
class InstancedField {
private:
    GLuint vao = 0, instVBO = 0, prog = 0, depthProg = 0;
    GLint uViewProj = -1, uTex = -1, uLightDir = -1, uLightSpace = -1, uShadow = -1;
    GLint uDepthLightSpace = -1;
    GLsizei indexCount = 0;
    int count = 0;

public:
    void init(const std::shared_ptr<Mesh>& mesh, const std::vector<glm::mat4>& mats)
    {
        count = (int)mats.size();
        indexCount = mesh->indexCount;

        prog = tools::linkProgramFiles("assets/shaders/inst.vert", "assets/shaders/inst.frag");
        uViewProj = glGetUniformLocation(prog, "uViewProj");
        uTex = glGetUniformLocation(prog, "uTex");
        uLightDir = glGetUniformLocation(prog, "uLightDir");
        uLightSpace = glGetUniformLocation(prog, "uLightSpace");
        uShadow = glGetUniformLocation(prog, "uShadow");
        depthProg = tools::linkProgramFiles("assets/shaders/inst_depth.vert", "assets/shaders/empty.frag");
        uDepthLightSpace = glGetUniformLocation(depthProg, "uLightSpace");

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // Per-vertex attributes borrow the mesh's buffers (pos/normal/uv).
        glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
        const GLsizei stride = 11 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);

        // Per-instance model matrix (locations 4..7, advance once per instance).
        glGenBuffers(1, &instVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferData(GL_ARRAY_BUFFER, mats.size() * sizeof(glm::mat4), mats.data(), GL_STATIC_DRAW);
        for (int i = 0; i < 4; ++i)
        {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                                  (void*)(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(4 + i, 1);
        }

        glBindVertexArray(0);
    }

    // Shadow-pass: render all instances into the light's depth buffer (they cast).
    void drawDepth(const glm::mat4& lightSpace)
    {
        if (count == 0) return;
        glUseProgram(depthProg);
        glUniformMatrix4fv(uDepthLightSpace, 1, GL_FALSE, &lightSpace[0][0]);
        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0, count);
        glBindVertexArray(0);
    }

    void draw(const glm::mat4& viewProj, GLuint tex, const glm::vec3& lightDir,
              const glm::mat4& lightSpace, GLuint shadowTex)
    {
        if (count == 0) return;
        glUseProgram(prog);
        glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniformMatrix4fv(uLightSpace, 1, GL_FALSE, &lightSpace[0][0]);
        glUniform3fv(uLightDir, 1, &lightDir[0]);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex); glUniform1i(uTex, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadowTex); glUniform1i(uShadow, 1);
        glActiveTexture(GL_TEXTURE0);

        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0, count);
        glBindVertexArray(0);
    }

    void free()
    {
        if (instVBO) glDeleteBuffers(1, &instVBO);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
        if (depthProg) glDeleteProgram(depthProg);
    }
};

} // namespace smallgine
