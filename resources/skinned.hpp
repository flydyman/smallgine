#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

namespace smallgine {

// GPU linear-blend-skinned demo: a segmented cylinder driven by a joint chain
// that waves over time. Proves the skeletal-animation path (skin matrices,
// per-vertex joint indices/weights) without needing a rigged asset.
class SkinnedModel {
private:
    static const int JOINTS = 6;
    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0;
    GLint uViewProj = -1, uModel = -1, uJoints = -1, uLightDir = -1, uColor = -1;
    GLsizei indexCount = 0;
    float segLen = 0.42f;
    glm::mat4 invBind[JOINTS];
    glm::mat4 skin[JOINTS];
    glm::mat4 model{1.0f};

public:
    void init(const glm::vec3& worldPos)
    {
        model = glm::translate(glm::mat4(1.0f), worldPos);
        prog = tools::linkProgramFiles("assets/shaders/skin.vert", "assets/shaders/skin.frag");
        uViewProj = glGetUniformLocation(prog, "uViewProj");
        uModel = glGetUniformLocation(prog, "uModel");
        uJoints = glGetUniformLocation(prog, "uJoints");
        uLightDir = glGetUniformLocation(prog, "uLightDir");
        uColor = glGetUniformLocation(prog, "uColor");

        for (int i = 0; i < JOINTS; ++i)
            invBind[i] = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, i * segLen, 0.0f)));

        // Build an open cylinder: rings along +Y, each vertex weighted to 2 joints.
        const int rings = JOINTS * 2 + 1, sides = 12;
        const float radius = 0.14f, height = JOINTS * segLen;
        std::vector<float> v; // pos3, nrm3, uv2, jointIdx4, weight4 = 16
        for (int r = 0; r < rings; ++r)
        {
            float y = (float)r / (rings - 1) * height;
            float jf = y / segLen;
            int j0 = (int)std::floor(jf); if (j0 > JOINTS - 1) j0 = JOINTS - 1;
            int j1 = std::min(j0 + 1, JOINTS - 1);
            float w1 = jf - std::floor(jf), w0 = 1.0f - w1;
            for (int s = 0; s < sides; ++s)
            {
                float a = (float)s / sides * 6.2831853f;
                float cx = std::cos(a), sz = std::sin(a);
                v.insert(v.end(), { cx * radius, y, sz * radius, cx, 0.0f, sz,
                                    (float)s / sides, y / height,
                                    (float)j0, (float)j1, 0.0f, 0.0f,
                                    w0, w1, 0.0f, 0.0f });
            }
        }
        std::vector<unsigned int> idx;
        for (int r = 0; r < rings - 1; ++r)
            for (int s = 0; s < sides; ++s)
            {
                unsigned int a = r * sides + s, b = r * sides + (s + 1) % sides;
                unsigned int c = (r + 1) * sides + s, d = (r + 1) * sides + (s + 1) % sides;
                idx.insert(idx.end(), { a, c, b, b, c, d });
            }
        indexCount = (GLsizei)idx.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        const GLsizei stride = 16 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                     glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));   glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));   glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));   glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));  glEnableVertexAttribArray(4);
        glBindVertexArray(0);
    }

    // Animate the joint chain (bend each joint about Z, phase-shifted).
    void update(float time)
    {
        glm::mat4 world(1.0f);
        for (int i = 0; i < JOINTS; ++i)
        {
            float angle = std::sin(time * 2.2f + i * 0.7f) * 0.35f;
            glm::mat4 local = (i == 0)
                ? glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1))
                : glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, segLen, 0.0f)), angle, glm::vec3(0, 0, 1));
            world = world * local;
            skin[i] = world * invBind[i];
        }
    }

    void draw(const glm::mat4& viewProj, const glm::vec3& lightDir)
    {
        glUseProgram(prog);
        glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniformMatrix4fv(uModel, 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(uJoints, JOINTS, GL_FALSE, &skin[0][0][0]);
        glm::vec3 ld = glm::normalize(lightDir);
        glUniform3fv(uLightDir, 1, &ld[0]);
        glm::vec3 col(0.85f, 0.5f, 0.9f);
        glUniform3fv(uColor, 1, &col[0]);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    void free()
    {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
