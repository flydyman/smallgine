#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../core/node.hpp"   // EmitterSpec
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdlib>

namespace smallgine {

// Additive billboard particle system: CPU-simulated, instanced quads.
// One emitter fountains particles upward under gravity; dead ones respawn.
class ParticleSystem {
private:
    struct Particle { glm::vec3 pos; glm::vec3 vel; float life; float maxLife; float size; glm::vec3 color; };

    std::shared_ptr<Mesh> quad;
    GLuint vao = 0, instVBO = 0, prog = 0;
    GLint uViewProj = -1, uCamRight = -1, uCamUp = -1;
    std::vector<Particle> parts;
    std::vector<float> instData;   // 8 floats per particle: pos.xyz,size, rgb,life01
    glm::vec3 origin{0.0f};
    EmitterSpec spec;

    void respawn(Particle& p)
    {
        auto rnd = [](float a, float b) { return a + (b - a) * (float)(std::rand() % 1000) / 1000.0f; };
        p.pos = origin + glm::vec3(rnd(-0.15f, 0.15f), 0.0f, rnd(-0.15f, 0.15f));
        p.vel = glm::vec3(rnd(-spec.spread, spec.spread), spec.speed * rnd(0.85f, 1.3f),
                          rnd(-spec.spread, spec.spread));
        p.maxLife = spec.life * rnd(0.7f, 1.2f);
        p.life = p.maxLife;
        p.size = spec.size * rnd(0.6f, 1.4f);
        p.color = glm::mix(spec.colorA, spec.colorB, rnd(0.0f, 1.0f));
    }

public:
    void init(const glm::vec3& emitter, const EmitterSpec& s)
    {
        origin = emitter;
        spec = s;
        int n = s.count;
        quad = makeQuad();
        prog = tools::linkProgramFiles("assets/shaders/particle.vert", "assets/shaders/particle.frag");
        uViewProj = glGetUniformLocation(prog, "uViewProj");
        uCamRight = glGetUniformLocation(prog, "uCamRight");
        uCamUp = glGetUniformLocation(prog, "uCamUp");

        parts.resize(n);
        for (Particle& p : parts) { respawn(p); p.life = (float)(std::rand() % 100) / 100.0f * p.maxLife; }
        instData.resize(n * 8, 0.0f);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad->vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->ebo);

        glGenBuffers(1, &instVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferData(GL_ARRAY_BUFFER, instData.size() * sizeof(float), nullptr, GL_STREAM_DRAW);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(5);
        glVertexAttribDivisor(5, 1);
        glBindVertexArray(0);
    }

    void setOrigin(const glm::vec3& o) { origin = o; }

    void update(float dt)
    {
        for (size_t i = 0; i < parts.size(); ++i)
        {
            Particle& p = parts[i];
            p.life -= dt;
            if (p.life <= 0.0f) respawn(p);
            p.vel.y -= spec.gravity * dt;
            p.pos += p.vel * dt;
            float l01 = p.life / p.maxLife;
            float* d = &instData[i * 8];
            d[0] = p.pos.x; d[1] = p.pos.y; d[2] = p.pos.z; d[3] = p.size;
            d[4] = p.color.r; d[5] = p.color.g; d[6] = p.color.b; d[7] = l01;
        }
    }

    void draw(const glm::mat4& viewProj, const glm::vec3& camRight, const glm::vec3& camUp)
    {
        if (!prog) return;
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferData(GL_ARRAY_BUFFER, instData.size() * sizeof(float), instData.data(), GL_STREAM_DRAW);

        glUseProgram(prog);
        glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniform3fv(uCamRight, 1, &camRight[0]);
        glUniform3fv(uCamUp, 1, &camUp[0]);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);   // additive
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, quad->indexCount, GL_UNSIGNED_INT, (void*)0, (GLsizei)parts.size());
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void free()
    {
        if (instVBO) glDeleteBuffers(1, &instVBO);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
        if (quad) quad->free();
    }
};

} // namespace smallgine
