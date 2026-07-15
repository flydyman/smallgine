#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "glmjson.hpp"

namespace smallgine {

// Surface appearance: base color tint, texture, specular response.
struct Material
{
    glm::vec3 color{1.0f};   // multiplies the texture
    std::string texture;     // path; empty => engine default
    std::string normalMap;   // tangent-space normal map; empty => geometric normal
    std::string heightMap;   // grayscale height for parallax; empty => none
    float shininess = 32.0f; // specular exponent
    float specular = 0.5f;   // specular strength
    float alpha = 1.0f;      // opacity (< 1 => transparent pass)
    float parallax = 0.0f;   // parallax depth scale (0 => off)
};

inline void to_json(nlohmann::json& j, const Material& m)
{
    j = nlohmann::json{
        {"color", m.color},
        {"texture", m.texture},
        {"normalMap", m.normalMap},
        {"heightMap", m.heightMap},
        {"shininess", m.shininess},
        {"specular", m.specular},
        {"alpha", m.alpha},
        {"parallax", m.parallax},
    };
}

inline void from_json(const nlohmann::json& j, Material& m)
{
    if (j.contains("color")) m.color = j.at("color").get<glm::vec3>();
    m.texture = j.value("texture", std::string());
    m.normalMap = j.value("normalMap", std::string());
    m.heightMap = j.value("heightMap", std::string());
    m.shininess = j.value("shininess", 32.0f);
    m.specular = j.value("specular", 0.5f);
    m.alpha = j.value("alpha", 1.0f);
    m.parallax = j.value("parallax", 0.0f);
}

} // namespace smallgine
