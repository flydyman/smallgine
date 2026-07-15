#pragma once
#include "../platform/glcontext.hpp"
#include "../resources/mesh.hpp"
#include "../tools/helpers.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>

namespace smallgine {

// Small structure-of-arrays ECS running alongside the Node scene graph.
// Components live in parallel dense arrays; systems iterate index ranges
// (so movement can be split across the job system).
class EcsWorld {
public:
    struct Transform { glm::vec3 pos{0.0f}, rot{0.0f}, scale{1.0f}; };
    struct Velocity  { glm::vec3 v{0.0f}; };
    struct Spin      { glm::vec3 degPerSec{0.0f}; };
    struct Renderable { std::shared_ptr<Mesh> mesh; GLuint tex = 0; glm::vec3 color{1.0f}; };

private:
    std::vector<Transform> tf;
    std::vector<Velocity> vel;
    std::vector<Spin> spin;
    std::vector<Renderable> rend;
    glm::vec3 boundsMin{-4.5f, -1.3f, -4.5f}, boundsMax{4.5f, 3.0f, 4.5f};

public:
    int count() const { return (int)tf.size(); }

    int add(const Transform& t, const Velocity& v, const Spin& s, const Renderable& r)
    {
        tf.push_back(t); vel.push_back(v); spin.push_back(s); rend.push_back(r);
        return (int)tf.size() - 1;
    }

    // Movement system chunk (bounce inside the bounds box). Thread-safe per range.
    void move(int begin, int end, float dt)
    {
        for (int i = begin; i < end; ++i)
        {
            glm::vec3& p = tf[i].pos; glm::vec3& v = vel[i].v;
            p += v * dt;
            for (int a = 0; a < 3; ++a)
            {
                if (p[a] < boundsMin[a]) { p[a] = boundsMin[a]; v[a] = -v[a]; }
                else if (p[a] > boundsMax[a]) { p[a] = boundsMax[a]; v[a] = -v[a]; }
            }
        }
    }

    // Spin system (cheap, serial).
    void spinSystem(float dt) { for (size_t i = 0; i < tf.size(); ++i) tf[i].rot += spin[i].degPerSec * dt; }

    // Render system: draw every renderable with the lit program (uniforms set by caller).
    void render(const tools::DrawUniforms& u, GLint uSelected, const glm::mat4& viewProj)
    {
        for (size_t i = 0; i < tf.size(); ++i)
        {
            if (!rend[i].mesh) continue;
            glm::mat4 m(1.0f);
            m = glm::translate(m, tf[i].pos);
            m = glm::rotate(m, glm::radians(tf[i].rot.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(tf[i].rot.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(tf[i].rot.z), glm::vec3(0, 0, 1));
            m = glm::scale(m, tf[i].scale);
            glm::mat4 mvp = viewProj * m;
            glUniformMatrix4fv(u.mvp, 1, GL_FALSE, &mvp[0][0]);
            glUniformMatrix4fv(u.model, 1, GL_FALSE, &m[0][0]);
            glUniform3fv(u.color, 1, &rend[i].color[0]);
            glUniform1f(u.shininess, 32.0f);
            glUniform1f(u.specular, 0.4f);
            glUniform1f(u.alpha, 1.0f);
            glUniform1i(u.hasNormalMap, 0);
            glUniform1f(u.parallax, 0.0f);
            glUniform1i(u.isTerrain, 0);
            glUniform1f(u.metallic, 0.0f);
            glUniform1f(u.roughness, 0.5f);
            glUniform1i(uSelected, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rend[i].tex);
            rend[i].mesh->draw();
        }
    }
};

} // namespace smallgine
