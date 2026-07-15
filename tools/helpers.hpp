#pragma once
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <vector>
#include "../platform/glcontext.hpp"
#include "../core/scene.hpp"
#include "../core/node.hpp"

namespace tools {

    using smallgine::Node;
    using smallgine::Scene;

    // Per-draw uniform locations for the lit/textured shader.
    struct DrawUniforms
    {
        GLint mvp = -1;
        GLint model = -1;
        GLint color = -1;
        GLint shininess = -1;
        GLint specular = -1;
        GLint alpha = -1;
        GLint hasNormalMap = -1;
    };

    // A drawable node with its resolved world transform.
    struct DrawItem
    {
        const Node* node;
        glm::mat4 global;
    };

    // Flatten the scene graph into world-space draw items.
    static void collect(const Node& node, const glm::mat4& parentModel, std::vector<DrawItem>& out)
    {
        glm::mat4 global = parentModel * node.localMatrix();
        if (node.mesh) out.push_back({&node, global});
        for (const Node& child : node.Children)
        {
            collect(child, global, out);
        }
    }

    // Set per-node uniforms + textures and draw. Program/lights/view set by caller.
    static void drawItem(const DrawItem& it, const DrawUniforms& u, const glm::mat4& viewProj)
    {
        const Node& node = *it.node;
        glm::mat4 mvp = viewProj * it.global;
        glUniformMatrix4fv(u.mvp, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(u.model, 1, GL_FALSE, &it.global[0][0]);
        glUniform3fv(u.color, 1, &node.material.color[0]);
        glUniform1f(u.shininess, node.material.shininess);
        glUniform1f(u.specular, node.material.specular);
        glUniform1f(u.alpha, node.material.alpha);
        glUniform1i(u.hasNormalMap, node.normalTexId ? 1 : 0);
        if (node.normalTexId)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, node.normalTexId);
            glActiveTexture(GL_TEXTURE0);
        }
        glBindTexture(GL_TEXTURE_2D, node.texId); // unit 0 (active)
        node.mesh->draw();
    }

    // Depth-only traversal for the shadow pass: sets light-space MVP per node.
    static void DrawNodeDepth(const Node& node, const glm::mat4& parentModel,
                              GLint uLightMVP, const glm::mat4& lightSpace)
    {
        glm::mat4 global = parentModel * node.localMatrix();
        if (node.mesh)
        {
            glm::mat4 lm = lightSpace * global;
            glUniformMatrix4fv(uLightMVP, 1, GL_FALSE, &lm[0][0]);
            node.mesh->draw();
        }
        for (const Node& child : node.Children)
        {
            DrawNodeDepth(child, global, uLightMVP, lightSpace);
        }
    }

    static void DrawSceneDepth(const Scene& scene, GLint uLightMVP, const glm::mat4& lightSpace)
    {
        DrawNodeDepth(scene.MainNode, glm::mat4(1.0f), uLightMVP, lightSpace);
    }

}
