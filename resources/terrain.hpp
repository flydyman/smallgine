#pragma once
#include "../third_party/stb_image.h" // declarations; impl in stb_impl.cpp
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <iostream>

namespace smallgine {

// Build a grid mesh in unit space ([-0.5,0.5] in x/z) whose height comes from a
// grayscale heightmap. Node scale places it in the world. Normals from neighbors.
inline std::shared_ptr<Mesh> makeTerrain(const std::string& heightPath, int grid = 64,
                                         float heightScale = 0.18f,
                                         std::vector<float>* outHeights = nullptr)
{
    int w = 0, h = 0, ch = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* img = stbi_load(heightPath.c_str(), &w, &h, &ch, 1);

    auto sample = [&](int gx, int gz) -> float {
        if (!img) return 0.0f;
        int ix = (int)((float)gx / (grid - 1) * (w - 1));
        int iz = (int)((float)gz / (grid - 1) * (h - 1));
        ix = ix < 0 ? 0 : (ix >= w ? w - 1 : ix);
        iz = iz < 0 ? 0 : (iz >= h ? h - 1 : iz);
        return img[iz * w + ix] / 255.0f * heightScale;
    };

    std::vector<float> verts;               // pos(3)+normal(3)+uv(2)
    verts.reserve(grid * grid * 8);
    if (outHeights) outHeights->assign(grid * grid, 0.0f);
    for (int z = 0; z < grid; ++z)
    {
        for (int x = 0; x < grid; ++x)
        {
            float fx = (float)x / (grid - 1) - 0.5f;
            float fz = (float)z / (grid - 1) - 0.5f;
            float hy = sample(x, z);
            if (outHeights) (*outHeights)[z * grid + x] = hy;
            // Normal from central differences of neighbor heights.
            float hl = sample(x - 1, z), hr = sample(x + 1, z);
            float hd = sample(x, z - 1), hu = sample(x, z + 1);
            float step = 1.0f / (grid - 1);
            glm::vec3 n = glm::normalize(glm::vec3(hl - hr, 2.0f * step, hd - hu));
            verts.insert(verts.end(), { fx, hy, fz, n.x, n.y, n.z,
                                        (float)x / (grid - 1) * 4.0f, (float)z / (grid - 1) * 4.0f });
        }
    }

    std::vector<unsigned int> idx;
    idx.reserve((grid - 1) * (grid - 1) * 6);
    for (int z = 0; z < grid - 1; ++z)
    {
        for (int x = 0; x < grid - 1; ++x)
        {
            unsigned int a = z * grid + x, b = a + 1, c = a + grid, d = c + 1;
            idx.insert(idx.end(), { a, c, b,  b, c, d });
        }
    }

    if (img) stbi_image_free(img);
    else std::cout << "Terrain heightmap missing (" << heightPath << "), flat grid" << std::endl;

    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

} // namespace smallgine
