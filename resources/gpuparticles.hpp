#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdlib>

namespace smallgine {

// Compute-shader particle system: state lives in an SSBO, integrated on the GPU
// (gravity + ground collision + respawn), drawn as additive points that read
// the same SSBO in the vertex shader. No CPU per-particle work each frame.
class GpuParticles {
private:
    GLuint ssbo = 0, updateProg = 0, drawProg = 0, vao = 0;
    GLint uDt = -1, uGround = -1, uCount = -1, uOrigin = -1, uVP = -1;
    int count = 0;
    float ground = -1.5f;
    glm::vec3 origin{0.0f, 0.5f, 0.0f};

public:
    bool ready() const { return updateProg && drawProg; }

    void init(int n, const glm::vec3& fountain, float groundY)
    {
        count = n; origin = fountain; ground = groundY;
        updateProg = tools::linkComputeFile("assets/shaders/particle_gpu.comp");
        drawProg = tools::linkProgramFiles("assets/shaders/particle_gpu.vert", "assets/shaders/particle_gpu.frag");
        uDt = glGetUniformLocation(updateProg, "uDt");
        uGround = glGetUniformLocation(updateProg, "uGround");
        uCount = glGetUniformLocation(updateProg, "uCount");
        uOrigin = glGetUniformLocation(updateProg, "uOrigin");
        uVP = glGetUniformLocation(drawProg, "uViewProj");

        // 8 floats per particle: pos.xyz, life, vel.xyz, pad (std430 vec4+vec4).
        std::vector<float> data(n * 8);
        auto rnd = []() { return (float)(std::rand() % 1000) / 1000.0f; };
        for (int i = 0; i < n; ++i)
        {
            float* d = &data[i * 8];
            d[0] = origin.x + (rnd() - 0.5f) * 0.4f; d[1] = origin.y; d[2] = origin.z + (rnd() - 0.5f) * 0.4f;
            d[3] = rnd() * 3.0f; // life (staggered)
            d[4] = (rnd() - 0.5f) * 2.2f; d[5] = 2.4f + rnd() * 2.4f; d[6] = (rnd() - 0.5f) * 2.2f; d[7] = 0.0f;
        }
        glGenBuffers(1, &ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glGenVertexArrays(1, &vao); // empty; vertex shader indexes the SSBO by gl_VertexID
    }

    void update(float dt)
    {
        if (!ready()) return;
        glUseProgram(updateProg);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
        glUniform1f(uDt, dt);
        glUniform1f(uGround, ground);
        glUniform1ui(uCount, (GLuint)count);
        glUniform3fv(uOrigin, 1, &origin[0]);
        glDispatchCompute((count + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // updates visible to the vertex fetch
    }

    void draw(const glm::mat4& viewProj)
    {
        if (!ready()) return;
        glUseProgram(drawProg);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
        glUniformMatrix4fv(uVP, 1, GL_FALSE, &viewProj[0][0]);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, count);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void free()
    {
        if (ssbo) glDeleteBuffers(1, &ssbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (updateProg) glDeleteProgram(updateProg);
        if (drawProg) glDeleteProgram(drawProg);
    }
};

} // namespace smallgine
