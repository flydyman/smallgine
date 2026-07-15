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
    float metallic = 0.0f;   // PBR: 0 = dielectric, 1 = metal
    float roughness = 0.5f;  // PBR: microfacet roughness
    bool terrain = false;    // slope-blend albedo between rockColor (steep) and color (flat)
    glm::vec3 rockColor{0.35f, 0.32f, 0.30f}; // steep-slope tint when terrain is set
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
        {"metallic", m.metallic},
        {"roughness", m.roughness},
        {"terrain", m.terrain},
        {"rockColor", m.rockColor},
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
    m.metallic = j.value("metallic", 0.0f);
    m.roughness = j.value("roughness", 0.5f);
    m.terrain = j.value("terrain", false);
    if (j.contains("rockColor")) m.rockColor = j.at("rockColor").get<glm::vec3>();
}

} // namespace smallgine
