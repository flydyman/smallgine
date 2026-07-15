#pragma once
#include <glm/glm.hpp>
#include <cmath>

namespace tools {

// View-frustum built from a view-projection matrix (Gribb-Hartmann).
struct Frustum
{
    glm::vec4 planes[6]; // xyz = normal, w = distance

    void fromMatrix(const glm::mat4& m)
    {
        auto row = [&](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
        planes[0] = row(3) + row(0); // left
        planes[1] = row(3) - row(0); // right
        planes[2] = row(3) + row(1); // bottom
        planes[3] = row(3) - row(1); // top
        planes[4] = row(3) + row(2); // near
        planes[5] = row(3) - row(2); // far
        for (int i = 0; i < 6; ++i)
        {
            float len = glm::length(glm::vec3(planes[i]));
            if (len > 0.0f) planes[i] /= len;
        }
    }

    bool sphereInside(const glm::vec3& c, float r) const
    {
        for (int i = 0; i < 6; ++i)
        {
            if (glm::dot(glm::vec3(planes[i]), c) + planes[i].w < -r) return false;
        }
        return true;
    }
};

// Bounding radius of a unit-ish mesh under a world matrix (max axis scale).
inline float worldRadius(const glm::mat4& g)
{
    float sx = glm::length(glm::vec3(g[0]));
    float sy = glm::length(glm::vec3(g[1]));
    float sz = glm::length(glm::vec3(g[2]));
    return std::max(sx, std::max(sy, sz)) * 0.9f;
}

} // namespace tools
