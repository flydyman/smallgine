#pragma once
#include "../third_party/stb_image.h" // declarations; impl in stb_impl.cpp
#include "../platform/vfs.hpp"
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <iostream>

namespace smallgine {

// Build a grid mesh (unit x/z in [-0.5,0.5]) whose height comes from a sampler
// f(u,v) over the unit square [0,1]^2. Same vertex layout / normal scheme as
// makeTerrain, but driven by any procedural generator instead of an image. Node
// scale places it in the world; optional outHeights records the raw samples.
//
// paletteUV: when true the UVs encode (normalized height, steepness) instead of
// a tiled xz coordinate, so a "landscape palette" texture (see makeTerrainPalette)
// paints sand / grass / rock / snow by elevation and slope. heightAspect is the
// node's y-scale / xz-scale ratio, used to measure steepness in world space.
inline std::shared_ptr<Mesh> makeHeightMesh(int grid,
                                            const std::function<float(float, float)>& sampler,
                                            float heightScale = 1.0f,
                                            std::vector<float>* outHeights = nullptr,
                                            bool paletteUV = false,
                                            float heightAspect = 1.0f)
{
    auto H = [&](int gx, int gz) -> float {
        int cx = gx < 0 ? 0 : (gx >= grid ? grid - 1 : gx);
        int cz = gz < 0 ? 0 : (gz >= grid ? grid - 1 : gz);
        return sampler((float)cx / (grid - 1), (float)cz / (grid - 1)) * heightScale;
    };

    std::vector<float> verts;               // pos(3)+normal(3)+uv(2)
    verts.reserve(grid * grid * 8);
    if (outHeights) outHeights->assign(grid * grid, 0.0f);
    float invScale = heightScale != 0.0f ? 1.0f / heightScale : 0.0f;
    for (int z = 0; z < grid; ++z)
    {
        for (int x = 0; x < grid; ++x)
        {
            float fx = (float)x / (grid - 1) - 0.5f;
            float fz = (float)z / (grid - 1) - 0.5f;
            float hy = H(x, z);
            if (outHeights) (*outHeights)[z * grid + x] = hy;
            float step = 1.0f / (grid - 1);
            glm::vec3 n = glm::normalize(glm::vec3(H(x - 1, z) - H(x + 1, z), 2.0f * step,
                                                   H(x, z - 1) - H(x, z + 1)));
            float u, v;
            if (paletteUV)
            {
                // U = normalized elevation, V = steepness measured in world space.
                glm::vec3 wn = glm::normalize(glm::vec3(n.x * heightAspect, n.y,
                                                        n.z * heightAspect));
                u = glm::clamp(hy * invScale, 0.0f, 1.0f);
                v = glm::clamp(1.0f - wn.y, 0.0f, 1.0f);
            }
            else
            {
                u = (float)x / (grid - 1) * 4.0f;
                v = (float)z / (grid - 1) * 4.0f;
            }
            verts.insert(verts.end(), { fx, hy, fz, n.x, n.y, n.z, u, v });
        }
    }

    std::vector<unsigned int> idx;
    idx.reserve((grid - 1) * (grid - 1) * 6);
    for (int z = 0; z < grid - 1; ++z)
        for (int x = 0; x < grid - 1; ++x)
        {
            unsigned int a = z * grid + x, b = a + 1, c = a + grid, d = c + 1;
            idx.insert(idx.end(), { a, c, b,  b, c, d });
        }

    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

// Flat horizontal plane in unit space ([-0.5,0.5] in x/z, y = 0), normal +Y, UVs
// 0..1. Subdivided into sub x sub cells so it can carry a water material (ripples
// are shaded per-fragment, but the extra verts keep it ready for vertex waves).
inline std::shared_ptr<Mesh> makeWaterPlane(int sub = 8)
{
    if (sub < 1) sub = 1;
    std::vector<float> verts; // pos(3)+normal(3)+uv(2)
    verts.reserve((sub + 1) * (sub + 1) * 8);
    for (int z = 0; z <= sub; ++z)
        for (int x = 0; x <= sub; ++x)
        {
            float u = (float)x / sub, v = (float)z / sub;
            verts.insert(verts.end(), { u - 0.5f, 0.0f, v - 0.5f, 0.0f, 1.0f, 0.0f, u, v });
        }

    std::vector<unsigned int> idx;
    idx.reserve(sub * sub * 6);
    int row = sub + 1;
    for (int z = 0; z < sub; ++z)
        for (int x = 0; x < sub; ++x)
        {
            unsigned int a = z * row + x, b = a + 1, c = a + row, d = c + 1;
            idx.insert(idx.end(), { a, c, b,  b, c, d });
        }

    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

// Build a grid mesh in unit space ([-0.5,0.5] in x/z) whose height comes from a
// grayscale heightmap. Node scale places it in the world. Normals from neighbors.
inline std::shared_ptr<Mesh> makeTerrain(const std::string& heightPath, int grid = 64,
                                         float heightScale = 0.18f,
                                         std::vector<float>* outHeights = nullptr)
{
    int w = 0, h = 0, ch = 0;
    stbi_set_flip_vertically_on_load(0);
    std::vector<unsigned char> bytes;
    unsigned char* img = readAsset(heightPath, bytes)
        ? stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &ch, 1)
        : nullptr;

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
