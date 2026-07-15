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

// Level-of-detail entry: use `mesh` when the node is within `maxDistance`.
struct LodLevel
{
    float maxDistance = 1e9f;
    std::string mesh;
};

inline void to_json(nlohmann::json& j, const LodLevel& l)
{
    j = nlohmann::json{{"maxDistance", l.maxDistance}, {"mesh", l.mesh}};
}
inline void from_json(const nlohmann::json& j, LodLevel& l)
{
    l.maxDistance = j.value("maxDistance", 1e9f);
    l.mesh = j.value("mesh", std::string());
}

// Data-driven particle emitter attached to a node (fountains from its origin).
struct EmitterSpec
{
    bool enabled = false;
    int count = 120;
    glm::vec3 colorA{0.9f, 0.45f, 0.1f}; // per-particle color randomized between A and B
    glm::vec3 colorB{1.0f, 0.8f, 0.3f};
    float size = 0.12f;
    float speed = 2.6f;   // upward launch speed
    float spread = 0.6f;  // lateral velocity spread
    float gravity = 2.0f;
    float life = 1.8f;
};

inline void to_json(nlohmann::json& j, const EmitterSpec& e)
{
    j = nlohmann::json{{"enabled", e.enabled}, {"count", e.count}, {"colorA", e.colorA},
                       {"colorB", e.colorB}, {"size", e.size}, {"speed", e.speed},
                       {"spread", e.spread}, {"gravity", e.gravity}, {"life", e.life}};
}
inline void from_json(const nlohmann::json& j, EmitterSpec& e)
{
    e.enabled = j.value("enabled", true); // presence of the block implies enabled
    e.count = j.value("count", 120);
    if (j.contains("colorA")) e.colorA = j.at("colorA").get<glm::vec3>();
    if (j.contains("colorB")) e.colorB = j.at("colorB").get<glm::vec3>();
    e.size = j.value("size", 0.12f);
    e.speed = j.value("speed", 2.6f);
    e.spread = j.value("spread", 0.6f);
    e.gravity = j.value("gravity", 2.0f);
    e.life = j.value("life", 1.8f);
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
    std::string MeshPath;     // OBJ/glTF file; empty => default cube
    std::vector<LodLevel> Lod; // optional distance-based mesh swaps
    bool Dynamic = false;      // physics: gravity + collision as a rigid body
    EmitterSpec Emitter;       // optional particle emitter
    std::vector<Node> Children;
    std::shared_ptr<Mesh> mesh; // resolved geometry; not serialized
    std::vector<std::shared_ptr<Mesh>> lodMeshes; // resolved Lod meshes; not serialized
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
        {"lod", n.Lod},
        {"dynamic", n.Dynamic},
        {"children", n.Children},
    };
    if (n.Emitter.enabled) j["emitter"] = n.Emitter;
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
    if (j.contains("lod")) n.Lod = j.at("lod").get<std::vector<LodLevel>>();
    n.Dynamic = j.value("dynamic", false);
    if (j.contains("emitter")) n.Emitter = j.at("emitter").get<EmitterSpec>();
    if (j.contains("children")) n.Children = j.at("children").get<std::vector<Node>>();
}

} // namespace smallgine
