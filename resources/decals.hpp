#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

namespace smallgine {

// Ground decals: alpha-blended quads laid flat on the terrain/ground, drawn
// instanced with a procedural splat texture. Dropped by triggers or on demand.
class Decals {
private:
    GLuint vao = 0, vbo = 0, ebo = 0, instVBO = 0, prog = 0, tex = 0;
    GLint uViewProj = -1, uTex = -1, uTint = -1;
    std::vector<float> inst;       // 4 floats per decal: x,y,z,size
    glm::vec3 tint{0.10f, 0.07f, 0.05f}; // dark scorch (won't bloom)
    static const int MAX = 64;

public:
    void init()
    {
        prog = tools::linkProgramFiles("assets/shaders/decal.vert", "assets/shaders/decal.frag");
        uViewProj = glGetUniformLocation(prog, "uViewProj");
        uTex = glGetUniformLocation(prog, "uTex");
        uTint = glGetUniformLocation(prog, "uTint");

        const float quad[12] = { -0.5f,0,-0.5f,  0.5f,0,-0.5f,  0.5f,0,0.5f,  -0.5f,0,0.5f };
        const unsigned int idx[6] = { 0,1,2, 2,3,0 };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo); glGenBuffers(1, &ebo); glGenBuffers(1, &instVBO);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferData(GL_ARRAY_BUFFER, MAX * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);
        glBindVertexArray(0);

        // Procedural soft splat texture (RGBA, alpha falloff + faint ring).
        const int S = 64;
        std::vector<unsigned char> px(S * S * 4);
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float dx = (x + 0.5f) / S * 2.0f - 1.0f, dy = (y + 0.5f) / S * 2.0f - 1.0f;
                float d = std::sqrt(dx * dx + dy * dy);
                float a = std::max(0.0f, 1.0f - d);
                a = a * a * (0.7f + 0.3f * std::sin(d * 18.0f)); // soft body + faint rings
                int i = (y * S + x) * 4;
                px[i] = px[i+1] = px[i+2] = 255;
                px[i+3] = (unsigned char)(std::min(1.0f, std::max(0.0f, a)) * 255.0f);
            }
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void add(const glm::vec3& pos, float size)
    {
        if ((int)(inst.size() / 4) >= MAX) inst.erase(inst.begin(), inst.begin() + 4); // ring
        inst.insert(inst.end(), { pos.x, pos.y, pos.z, size });
    }
    int count() const { return (int)(inst.size() / 4); }

    void draw(const glm::mat4& viewProj)
    {
        if (inst.empty()) return;
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size() * sizeof(float), inst.data());
        glUseProgram(prog);
        glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniform3fv(uTint, 1, &tint[0]);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex); glUniform1i(uTex, 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0, count());
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void free()
    {
        if (instVBO) glDeleteBuffers(1, &instVBO);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (tex) glDeleteTextures(1, &tex);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
