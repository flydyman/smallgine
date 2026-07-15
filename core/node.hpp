#pragma once
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glmjson.hpp"
#include "material.hpp"
#include "../resources/mesh.hpp"

namespace smallgine {

struct Node
{
    unsigned long id = 0;
    std::string Name;
    glm::vec3 Position{0.0f};
    glm::vec3 Rotation{0.0f}; // euler degrees (x, y, z)
    glm::vec3 Scale{1.0f};
    glm::vec3 Spin{0.0f};     // per-node angular velocity, degrees/sec (behavior hook)
    Material material;        // appearance
    std::string MeshPath;     // OBJ file; empty => default cube
    std::vector<Node> Children;
    std::shared_ptr<Mesh> mesh; // resolved geometry; not serialized
    GLuint texId = 0;           // resolved from material.texture; not serialized
    GLuint normalTexId = 0;     // resolved from material.normalMap; not serialized
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
    n.materialSet = j.contains("material");
    if (n.materialSet)          n.material  = j.at("material").get<Material>();
    n.MeshPath = j.value("mesh", std::string());
    if (j.contains("children")) n.Children = j.at("children").get<std::vector<Node>>();
}

} // namespace smallgine
