#pragma once
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

// JSON <-> glm::vec3 (found via ADL in the glm namespace).
namespace glm {
    inline void to_json(nlohmann::json& j, const glm::vec3& v)
    {
        j = { v.x, v.y, v.z };
    }
    inline void from_json(const nlohmann::json& j, glm::vec3& v)
    {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }
}
