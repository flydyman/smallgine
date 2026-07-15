#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <tuple>
#include <iostream>
#include <glm/glm.hpp>
#include "mesh.hpp"
#include "../core/material.hpp"

namespace smallgine {

// Parse a .mtl file, filling the first material found (Kd color, map_Kd texture).
// Returns true if a material was read. `objDir` prefixes a relative map_Kd path.
inline bool loadMTL(const std::string& path, const std::string& objDir, Material& out)
{
    std::ifstream f(path);
    if (!f) return false;

    bool found = false;
    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "newmtl")
        {
            if (found) break; // only the first material
            found = true;
        }
        else if (tag == "Kd")  { ss >> out.color.x >> out.color.y >> out.color.z; }
        else if (tag == "Ns")  { ss >> out.shininess; }
        else if (tag == "Ks")
        {
            glm::vec3 ks(0.0f);
            ss >> ks.x >> ks.y >> ks.z;
            out.specular = (ks.x + ks.y + ks.z) / 3.0f; // grayscale specular strength
        }
        else if (tag == "map_Kd")
        {
            std::string tex;
            ss >> tex;
            // keep engine-resolvable paths as-is; prefix bare filenames with obj dir
            out.texture = (tex.find('/') == std::string::npos && !objDir.empty())
                          ? objDir + "/" + tex : tex;
        }
    }
    return found;
}

// Minimal Wavefront OBJ loader: v / vt / vn / f (any of v, v/vt, v//vn, v/vt/vn),
// polygons fan-triangulated. Missing normals are computed per face. Returns
// nullptr on failure. Requires a current GL context (builds the Mesh).
inline std::shared_ptr<Mesh> loadOBJ(const std::string& path,
                                     Material* outMat = nullptr, bool* outHasMat = nullptr)
{
    std::ifstream f(path);
    if (!f)
    {
        std::cout << "OBJ load failed: " << path << std::endl;
        return nullptr;
    }

    std::string objDir;
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos) objDir = path.substr(0, slash);
    std::string mtlLib;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec3> normals;

    struct Ref { int p, t, n; }; // 1-based OBJ indices (0 = absent)
    std::vector<Ref> tris;       // flat, 3 refs per triangle

    auto parseRef = [](const std::string& tok) -> Ref {
        Ref r{0, 0, 0};
        size_t s1 = tok.find('/');
        if (s1 == std::string::npos) { r.p = std::stoi(tok); return r; }
        r.p = std::stoi(tok.substr(0, s1));
        size_t s2 = tok.find('/', s1 + 1);
        if (s2 == std::string::npos) { r.t = std::stoi(tok.substr(s1 + 1)); return r; }
        if (s2 > s1 + 1) r.t = std::stoi(tok.substr(s1 + 1, s2 - s1 - 1));
        std::string ns = tok.substr(s2 + 1);
        if (!ns.empty()) r.n = std::stoi(ns);
        return r;
    };

    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "mtllib") { ss >> mtlLib; }
        else if (tag == "v")  { glm::vec3 v; ss >> v.x >> v.y >> v.z; positions.push_back(v); }
        else if (tag == "vt") { glm::vec2 t; ss >> t.x >> t.y; texcoords.push_back(t); }
        else if (tag == "vn") { glm::vec3 n; ss >> n.x >> n.y >> n.z; normals.push_back(n); }
        else if (tag == "f")
        {
            std::vector<Ref> poly;
            std::string tok;
            while (ss >> tok) poly.push_back(parseRef(tok));
            for (size_t i = 1; i + 1 < poly.size(); ++i)
            {
                tris.push_back(poly[0]);
                tris.push_back(poly[i]);
                tris.push_back(poly[i + 1]);
            }
        }
    }

    auto resolve = [](int i, size_t count) -> int {
        if (i > 0) return i - 1;               // 1-based
        if (i < 0) return (int)count + i;      // negative = from end
        return -1;                              // absent
    };

    std::vector<float> verts;
    std::vector<unsigned int> idx;
    std::map<std::tuple<int, int, int>, unsigned int> cache;

    for (size_t k = 0; k < tris.size(); k += 3)
    {
        int pi[3], ti[3], ni[3];
        glm::vec3 p[3];
        for (int j = 0; j < 3; ++j)
        {
            const Ref& r = tris[k + j];
            pi[j] = resolve(r.p, positions.size());
            ti[j] = r.t ? resolve(r.t, texcoords.size()) : -1;
            ni[j] = r.n ? resolve(r.n, normals.size()) : -1;
            p[j] = positions[pi[j]];
        }
        glm::vec3 faceN = glm::normalize(glm::cross(p[1] - p[0], p[2] - p[0]));

        for (int j = 0; j < 3; ++j)
        {
            glm::vec3 nrm = (ni[j] >= 0) ? normals[ni[j]] : faceN;
            glm::vec2 uv  = (ti[j] >= 0) ? texcoords[ti[j]] : glm::vec2(0.0f);
            auto key = std::make_tuple(pi[j], ti[j], ni[j]);
            auto it = cache.find(key);
            unsigned int out;
            if (it != cache.end())
            {
                out = it->second;
            }
            else
            {
                out = (unsigned int)(verts.size() / 8);
                verts.insert(verts.end(), {
                    p[j].x, p[j].y, p[j].z,
                    nrm.x, nrm.y, nrm.z,
                    uv.x, uv.y
                });
                cache[key] = out;
            }
            idx.push_back(out);
        }
    }

    if (verts.empty())
    {
        std::cout << "OBJ has no geometry: " << path << std::endl;
        return nullptr;
    }

    if (outMat && !mtlLib.empty())
    {
        std::string mtlPath = objDir.empty() ? mtlLib : objDir + "/" + mtlLib;
        bool ok = loadMTL(mtlPath, objDir, *outMat);
        if (outHasMat) *outHasMat = ok;
    }

    std::cout << "OBJ loaded: " << path << " (" << (verts.size() / 8)
              << " verts, " << (idx.size() / 3) << " tris)" << std::endl;
    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

} // namespace smallgine
