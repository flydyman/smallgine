#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "node.hpp"
#include "light.hpp"

namespace smallgine {

struct Scene {
    std::string name;
    std::string description;
    Node MainNode;
    std::vector<Light> Lights;
};

inline void to_json(nlohmann::json& j, const Scene& s)
{
    j = nlohmann::json{
        {"name", s.name},
        {"description", s.description},
        {"root", s.MainNode},
        {"lights", s.Lights},
    };
}

inline void from_json(const nlohmann::json& j, Scene& s)
{
    s.name = j.value("name", std::string());
    s.description = j.value("description", std::string());
    if (j.contains("root")) s.MainNode = j.at("root").get<Node>();
    if (j.contains("lights")) s.Lights = j.at("lights").get<std::vector<Light>>();
}

inline Scene loadSceneFromString(const std::string& text)
{
    return nlohmann::json::parse(text).get<Scene>();
}

} // namespace smallgine
