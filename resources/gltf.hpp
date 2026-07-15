#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iostream>
#include "mesh.hpp"

namespace smallgine {

// Decode a base64 string (glTF data-URI buffers).
inline std::vector<unsigned char> base64Decode(const std::string& s)
{
    static const std::string T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    for (int i = 0; i < 64; ++i) rev[(unsigned char)T[i]] = i;

    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : s)
    {
        if (rev[c] == -1) continue;
        val = (val << 6) + rev[c];
        bits += 6;
        if (bits >= 0) { out.push_back((unsigned char)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

// Minimal glTF 2.0 loader: first mesh primitive, float POSITION/NORMAL/TEXCOORD_0,
// ushort/uint/ubyte indices, single embedded (data-URI) buffer, tightly packed.
inline std::shared_ptr<Mesh> loadGLTF(const std::string& path)
{
    std::ifstream f(path);
    if (!f) { std::cout << "glTF load failed: " << path << std::endl; return nullptr; }
    nlohmann::json j;
    try { f >> j; }
    catch (const std::exception& e) { std::cout << "glTF parse error: " << e.what() << std::endl; return nullptr; }

    if (!j.contains("buffers") || !j.contains("accessors") || !j.contains("meshes"))
    {
        std::cout << "glTF missing sections: " << path << std::endl; return nullptr;
    }

    std::string uri = j["buffers"][0].value("uri", std::string());
    auto comma = uri.find(',');
    if (comma == std::string::npos) { std::cout << "glTF: only embedded buffers supported" << std::endl; return nullptr; }
    std::vector<unsigned char> buf = base64Decode(uri.substr(comma + 1));

    const auto& accessors = j["accessors"];
    const auto& views = j["bufferViews"];
    const auto& prim = j["meshes"][0]["primitives"][0];
    const auto& attr = prim["attributes"];

    auto viewOffset = [&](int acc) {
        int bv = accessors[acc]["bufferView"].get<int>();
        int off = views[bv].value("byteOffset", 0) + accessors[acc].value("byteOffset", 0);
        return off;
    };
    auto readFloats = [&](int acc, int comps) {
        int count = accessors[acc]["count"].get<int>();
        int off = viewOffset(acc);
        std::vector<float> v(count * comps);
        std::memcpy(v.data(), &buf[off], count * comps * sizeof(float));
        return v;
    };

    int posA = attr["POSITION"].get<int>();
    int count = accessors[posA]["count"].get<int>();
    std::vector<float> pos = readFloats(posA, 3);
    std::vector<float> nrm = attr.contains("NORMAL")     ? readFloats(attr["NORMAL"].get<int>(), 3) : std::vector<float>(count * 3, 0.0f);
    std::vector<float> uv  = attr.contains("TEXCOORD_0") ? readFloats(attr["TEXCOORD_0"].get<int>(), 2) : std::vector<float>(count * 2, 0.0f);

    std::vector<unsigned int> idx;
    if (prim.contains("indices"))
    {
        int acc = prim["indices"].get<int>();
        int ic = accessors[acc]["count"].get<int>();
        int ct = accessors[acc]["componentType"].get<int>();
        int off = viewOffset(acc);
        for (int i = 0; i < ic; ++i)
        {
            if (ct == 5123)      { uint16_t x; std::memcpy(&x, &buf[off + i * 2], 2); idx.push_back(x); }
            else if (ct == 5125) { uint32_t x; std::memcpy(&x, &buf[off + i * 4], 4); idx.push_back(x); }
            else                 { idx.push_back(buf[off + i]); }
        }
    }
    else { for (int i = 0; i < count; ++i) idx.push_back((unsigned int)i); }

    std::vector<float> verts(count * 8);
    for (int i = 0; i < count; ++i)
    {
        verts[i*8+0] = pos[i*3+0]; verts[i*8+1] = pos[i*3+1]; verts[i*8+2] = pos[i*3+2];
        verts[i*8+3] = nrm[i*3+0]; verts[i*8+4] = nrm[i*3+1]; verts[i*8+5] = nrm[i*3+2];
        verts[i*8+6] = uv[i*2+0];  verts[i*8+7] = uv[i*2+1];
    }

    std::cout << "glTF loaded: " << path << " (" << count << " verts, "
              << (idx.size() / 3) << " tris)" << std::endl;
    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

} // namespace smallgine
