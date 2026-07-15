#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glmjson.hpp"
#include "material.hpp"
#include "../resources/mesh.hpp"

namespace smallgine {

// A single animation keyframe: local transform at time t (seconds).
struct Keyframe
{
    float t = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

inline void to_json(nlohmann::json& j, const Keyframe& k)
{
    j = nlohmann::json{{"t", k.t}, {"position", k.position}, {"rotation", k.rotation}, {"scale", k.scale}};
}
inline void from_json(const nlohmann::json& j, Keyframe& k)
{
    k.t = j.value("t", 0.0f);
    if (j.contains("position")) k.position = j.at("position").get<glm::vec3>();
    if (j.contains("rotation")) k.rotation = j.at("rotation").get<glm::vec3>();
    if (j.contains("scale"))    k.scale    = j.at("scale").get<glm::vec3>();
}

struct Node
{
    unsigned long id = 0;
    std::string Name;
    glm::vec3 Position{0.0f};
    glm::vec3 Rotation{0.0f}; // euler degrees (x, y, z)
    glm::vec3 Scale{1.0f};
    glm::vec3 Spin{0.0f};     // per-node angular velocity, degrees/sec (behavior hook)
    std::vector<Keyframe> Animation; // if non-empty, drives local transform (looped)
    Material material;        // appearance
    std::string MeshPath;     // OBJ file; empty => default cube
    std::vector<Node> Children;
    std::shared_ptr<Mesh> mesh; // resolved geometry; not serialized
    GLuint texId = 0;           // resolved from material.texture; not serialized
    GLuint normalTexId = 0;     // resolved from material.normalMap; not serialized
    GLuint heightTexId = 0;     // resolved from material.heightMap; not serialized
    bool materialSet = false;   // did JSON specify a material block?; not serialized

    // Depth-first search by name (nullptr if absent).
    Node* find(const std::string& name)
    {
        if (Name == name) return this;
        for (Node& c : Children) { if (Node* r = c.find(name)) return r; }
        return nullptr;
    }

    // Depth-first search by id.
    Node* findById(unsigned long wanted)
    {
        if (id == wanted) return this;
        for (Node& c : Children) { if (Node* r = c.findById(wanted)) return r; }
        return nullptr;
    }

    void addChild(const Node& n) { Children.push_back(n); }

    bool removeChild(const std::string& name)
    {
        for (auto it = Children.begin(); it != Children.end(); ++it)
        {
            if (it->Name == name) { Children.erase(it); return true; }
        }
        return false;
    }

    // Sample the animation (looped) and write into Position/Rotation/Scale.
    void evalAnimation(float time)
    {
        if (Animation.size() < 2) return;
        float dur = Animation.back().t;
        if (dur <= 0.0f) return;
        float tt = std::fmod(time, dur);
        for (size_t i = 0; i + 1 < Animation.size(); ++i)
        {
            const Keyframe& a = Animation[i];
            const Keyframe& b = Animation[i + 1];
            if (tt >= a.t && tt <= b.t)
            {
                float f = (b.t > a.t) ? (tt - a.t) / (b.t - a.t) : 0.0f;
                Position = glm::mix(a.position, b.position, f);
                Rotation = glm::mix(a.rotation, b.rotation, f);
                Scale = glm::mix(a.scale, b.scale, f);
                return;
            }
        }
    }

    // Local transform relative to parent: T * R * S.
    glm::mat4 localMatrix() const
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, Position);
        m = glm::rotate(m, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::rotate(m, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::scale(m, Scale);
        return m;
    }
};

inline void to_json(nlohmann::json& j, const Node& n)
{
    j = nlohmann::json{
        {"id", n.id},
        {"name", n.Name},
        {"position", n.Position},
        {"rotation", n.Rotation},
        {"scale", n.Scale},
        {"spin", n.Spin},
        {"animation", n.Animation},
        {"material", n.material},
        {"mesh", n.MeshPath},
        {"children", n.Children},
    };
}

inline void from_json(const nlohmann::json& j, Node& n)
{
    n.id = j.value("id", 0ul);
    n.Name = j.value("name", std::string());
    if (j.contains("position")) n.Position = j.at("position").get<glm::vec3>();
    if (j.contains("rotation")) n.Rotation = j.at("rotation").get<glm::vec3>();
    if (j.contains("scale"))    n.Scale    = j.at("scale").get<glm::vec3>();
    if (j.contains("spin"))     n.Spin     = j.at("spin").get<glm::vec3>();
    if (j.contains("animation")) n.Animation = j.at("animation").get<std::vector<Keyframe>>();
    n.materialSet = j.contains("material");
    if (n.materialSet)          n.material  = j.at("material").get<Material>();
    n.MeshPath = j.value("mesh", std::string());
    if (j.contains("children")) n.Children = j.at("children").get<std::vector<Node>>();
}

} // namespace smallgine
