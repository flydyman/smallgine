#pragma once
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "glmjson.hpp"

namespace smallgine {

// A light. Directional: `position` is the direction TO the light.
// Point: `position` is a world-space location.
struct Light
{
    int type = 0;              // 0 = directional, 1 = point
    glm::vec3 position{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

inline void to_json(nlohmann::json& j, const Light& l)
{
    j = nlohmann::json{
        {"type", l.type},
        {"position", l.position},
        {"color", l.color},
        {"intensity", l.intensity},
    };
}

inline void from_json(const nlohmann::json& j, Light& l)
{
    l.type = j.value("type", 0);
    if (j.contains("position")) l.position = j.at("position").get<glm::vec3>();
    if (j.contains("color")) l.color = j.at("color").get<glm::vec3>();
    l.intensity = j.value("intensity", 1.0f);
}

} // namespace smallgine
