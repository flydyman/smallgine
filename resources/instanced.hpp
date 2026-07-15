#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace smallgine {

namespace {
    const char* kInstVert =
        "#version 310 es\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec2 aUV;\n"
        "layout(location = 4) in vec4 aM0;\n"
        "layout(location = 5) in vec4 aM1;\n"
        "layout(location = 6) in vec4 aM2;\n"
        "layout(location = 7) in vec4 aM3;\n"
        "uniform mat4 uViewProj;\n"
        "out vec3 vNormal;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    mat4 model = mat4(aM0, aM1, aM2, aM3);\n"
        "    vNormal = mat3(model) * aNormal;\n"
        "    vUV = aUV;\n"
        "    gl_Position = uViewProj * model * vec4(aPos, 1.0);\n"
        "}\n";

    const char* kInstFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec3 vNormal;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec3 uLightDir;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    float d = max(dot(normalize(vNormal), uLightDir), 0.0);\n"
        "    vec3 c = texture(uTex, vUV).rgb * (0.3 + 0.7 * d);\n"
        "    FragColor = vec4(c, 1.0);\n"
        "}\n";
}

// Draws many copies of one mesh in a single call via instanced arrays.
class InstancedField {
private:
    GLuint vao = 0, instVBO = 0, prog = 0;
    GLint uViewProj = -1, uTex = -1, uLightDir = -1;
    GLsizei indexCount = 0;
    int count = 0;

public:
    void init(const std::shared_ptr<Mesh>& mesh, const std::vector<glm::mat4>& mats)
    {
        count = (int)mats.size();
        indexCount = mesh->indexCount;

        prog = tools::linkProgram(kInstVert, kInstFrag);
        uViewProj = glGetUniformLocation(prog, "uViewProj");
        uTex = glGetUniformLocation(prog, "uTex");
        uLightDir = glGetUniformLocation(prog, "uLightDir");

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

    void draw(const glm::mat4& viewProj, GLuint tex, const glm::vec3& lightDir)
    {
        if (count == 0) return;
        glUseProgram(prog);
        glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniform3fv(uLightDir, 1, &lightDir[0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(uTex, 0);

        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0, count);
        glBindVertexArray(0);
    }

    void free()
    {
        if (instVBO) glDeleteBuffers(1, &instVBO);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
