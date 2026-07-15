#pragma once
#include "../platform/glcontext.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace smallgine {

// Loads a rigged glTF/.glb (JOINTS_0/WEIGHTS_0 + skin inverse-bind matrices +
// node animation channels) and drives GPU linear-blend skinning per frame.
class SkinnedGLTF {
private:
    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0;
    GLsizei indexCount = 0;
    GLint uViewProj = -1, uModel = -1, uJoints = -1, uLightDir = -1, uColor = -1;

    struct GNode {
        glm::vec3 t{0.0f}, s{1.0f}, bt{0.0f}, bs{1.0f};
        glm::quat r{1, 0, 0, 0}, br{1, 0, 0, 0};   // current + bind-pose TRS
        std::vector<int> children;
    };
    std::vector<GNode> nodes;
    std::vector<int> sceneRoots;
    std::vector<int> joints;
    std::vector<glm::mat4> invBind;

    struct Chan { int node; int path; std::vector<float> times; std::vector<glm::vec4> values; };
    std::vector<Chan> channels;
    float animDur = 1.0f;

    std::vector<glm::mat4> jointMat;
    glm::mat4 model{1.0f};
    glm::vec3 color{0.35f, 0.85f, 0.95f};

    void computeGlobals(int node, const glm::mat4& parent, std::vector<glm::mat4>& out) const;

public:
    bool load(const std::string& path, const glm::vec3& worldPos);
    void update(float time);
    void draw(const glm::mat4& viewProj, const glm::vec3& lightDir);
    bool ready() const { return prog != 0 && indexCount > 0; }
    int jointCount() const { return (int)joints.size(); }
    void free();
};

} // namespace smallgine
