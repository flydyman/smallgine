#include "gltf.hpp"
#include "../platform/vfs.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <functional>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace smallgine {

std::vector<unsigned char> base64Decode(const std::string& s)
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

std::shared_ptr<Mesh> loadGLTF(const std::string& path, Material* outMat, bool* outHas)
{
    std::vector<unsigned char> file;
    if (!readAsset(path, file)) { std::cout << "glTF load failed: " << path << std::endl; return nullptr; }
    if (file.size() < 12) { std::cout << "glTF too small: " << path << std::endl; return nullptr; }

    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
    nlohmann::json j;
    std::vector<unsigned char> glbBin;
    bool isGlb = std::memcmp(file.data(), "glTF", 4) == 0;

    if (isGlb)
    {
        // Header (12B) then chunks: [len(4) type(4) data]. JSON then BIN.
        size_t p = 12;
        while (p + 8 <= file.size())
        {
            uint32_t len; std::memcpy(&len, &file[p], 4);
            uint32_t type; std::memcpy(&type, &file[p + 4], 4);
            const unsigned char* data = &file[p + 8];
            if (type == 0x4E4F534A) // "JSON"
                j = nlohmann::json::parse(std::string((const char*)data, len), nullptr, false);
            else if (type == 0x004E4942) // "BIN\0"
                glbBin.assign(data, data + len);
            p += 8 + len;
        }
        if (j.is_discarded()) { std::cout << "glTF(glb) JSON parse error: " << path << std::endl; return nullptr; }
    }
    else
    {
        try { j = nlohmann::json::parse(file.begin(), file.end()); }
        catch (const std::exception& e) { std::cout << "glTF parse error: " << e.what() << std::endl; return nullptr; }
    }

    if (!j.contains("accessors") || !j.contains("meshes")) { std::cout << "glTF missing sections: " << path << std::endl; return nullptr; }

    // Resolve all buffers (data-URI, external file, or the glb BIN chunk).
    std::vector<std::vector<unsigned char>> buffers;
    for (const auto& b : j["buffers"])
    {
        std::string uri = b.value("uri", std::string());
        if (uri.empty()) { buffers.push_back(glbBin); }
        else if (uri.rfind("data:", 0) == 0)
        {
            auto comma = uri.find(',');
            buffers.push_back(comma == std::string::npos ? std::vector<unsigned char>() : base64Decode(uri.substr(comma + 1)));
        }
        else
        {
            std::vector<unsigned char> buf;
            readAsset(dir + uri, buf); // sibling .bin via VFS (pack or loose)
            buffers.push_back(std::move(buf));
        }
    }

    const auto& accessors = j["accessors"];
    const auto& views = j["bufferViews"];

    auto readFloats = [&](int acc, int comps) {
        const auto& a = accessors[acc];
        const auto& bv = views[a["bufferView"].get<int>()];
        const std::vector<unsigned char>& buf = buffers[bv.value("buffer", 0)];
        int base = bv.value("byteOffset", 0) + a.value("byteOffset", 0);
        int stride = bv.value("byteStride", 0); if (stride == 0) stride = comps * 4;
        int count = a["count"].get<int>();
        std::vector<float> v(count * comps);
        for (int i = 0; i < count; ++i) std::memcpy(&v[i * comps], &buf[base + i * stride], comps * 4);
        return v;
    };
    auto readIndices = [&](int acc, std::vector<unsigned int>& out, unsigned int offset) {
        const auto& a = accessors[acc];
        const auto& bv = views[a["bufferView"].get<int>()];
        const std::vector<unsigned char>& buf = buffers[bv.value("buffer", 0)];
        int base = bv.value("byteOffset", 0) + a.value("byteOffset", 0);
        int ic = a["count"].get<int>(), ct = a["componentType"].get<int>();
        for (int i = 0; i < ic; ++i)
        {
            unsigned int v;
            if (ct == 5123)      { uint16_t x; std::memcpy(&x, &buf[base + i * 2], 2); v = x; }
            else if (ct == 5125) { uint32_t x; std::memcpy(&x, &buf[base + i * 4], 4); v = x; }
            else                 { v = buf[base + i]; }
            out.push_back(v + offset);
        }
    };

    // Node local transform: explicit matrix, or T * R(quat) * S.
    auto nodeMatrix = [&](const nlohmann::json& n) {
        if (n.contains("matrix"))
        {
            glm::mat4 m; auto a = n["matrix"];
            for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) m[c][r] = a[c * 4 + r].get<float>();
            return m;
        }
        glm::mat4 m(1.0f);
        if (n.contains("translation")) { auto t = n["translation"]; m = glm::translate(m, glm::vec3(t[0], t[1], t[2])); }
        if (n.contains("rotation")) { auto q = n["rotation"]; m *= glm::mat4_cast(glm::quat(q[3].get<float>(), q[0].get<float>(), q[1].get<float>(), q[2].get<float>())); }
        if (n.contains("scale")) { auto s = n["scale"]; m = glm::scale(m, glm::vec3(s[0], s[1], s[2])); }
        return m;
    };

    std::vector<float> verts;   // pos(3)+normal(3)+uv(2)
    std::vector<unsigned int> idx;
    int materialIndex = -1;

    std::function<void(int, const glm::mat4&)> processNode = [&](int ni, const glm::mat4& parent) {
        const auto& node = j["nodes"][ni];
        glm::mat4 world = parent * nodeMatrix(node);
        glm::mat3 nmat(glm::transpose(glm::inverse(world)));
        if (node.contains("mesh"))
        {
            for (const auto& prim : j["meshes"][node["mesh"].get<int>()]["primitives"])
            {
                const auto& attr = prim["attributes"];
                if (!attr.contains("POSITION")) continue;
                int posA = attr["POSITION"].get<int>();
                int count = accessors[posA]["count"].get<int>();
                std::vector<float> pos = readFloats(posA, 3);
                std::vector<float> nrm = attr.contains("NORMAL")     ? readFloats(attr["NORMAL"].get<int>(), 3) : std::vector<float>(count * 3, 0.0f);
                std::vector<float> uv  = attr.contains("TEXCOORD_0") ? readFloats(attr["TEXCOORD_0"].get<int>(), 2) : std::vector<float>(count * 2, 0.0f);
                unsigned int offset = (unsigned int)(verts.size() / 8);
                for (int i = 0; i < count; ++i)
                {
                    glm::vec3 p = glm::vec3(world * glm::vec4(pos[i*3], pos[i*3+1], pos[i*3+2], 1.0f));
                    glm::vec3 nn = glm::normalize(nmat * glm::vec3(nrm[i*3], nrm[i*3+1], nrm[i*3+2]));
                    verts.insert(verts.end(), { p.x, p.y, p.z, nn.x, nn.y, nn.z, uv[i*2], uv[i*2+1] });
                }
                if (prim.contains("indices")) readIndices(prim["indices"].get<int>(), idx, offset);
                else for (int i = 0; i < count; ++i) idx.push_back(offset + i);
                if (materialIndex < 0 && prim.contains("material")) materialIndex = prim["material"].get<int>();
            }
        }
        if (node.contains("children")) for (const auto& c : node["children"]) processNode(c.get<int>(), world);
    };

    // Traverse the default scene (or node 0 as a fallback).
    if (j.contains("scenes") && j.contains("scene"))
        for (const auto& n : j["scenes"][j["scene"].get<int>()]["nodes"]) processNode(n.get<int>(), glm::mat4(1.0f));
    else if (j.contains("nodes")) processNode(0, glm::mat4(1.0f));

    if (verts.empty()) { std::cout << "glTF: no geometry: " << path << std::endl; return nullptr; }

    // PBR material import (baseColor factor + texture, metallic/roughness).
    if (outMat && materialIndex >= 0 && j.contains("materials"))
    {
        const auto& pm = j["materials"][materialIndex];
        if (pm.contains("pbrMetallicRoughness"))
        {
            const auto& pbr = pm["pbrMetallicRoughness"];
            if (pbr.contains("baseColorFactor")) { auto b = pbr["baseColorFactor"]; outMat->color = glm::vec3(b[0], b[1], b[2]); outMat->alpha = b[3]; }
            outMat->metallic = pbr.value("metallicFactor", 1.0f);
            outMat->roughness = pbr.value("roughnessFactor", 1.0f);
            if (pbr.contains("baseColorTexture") && j.contains("images"))
            {
                int ti = pbr["baseColorTexture"]["index"].get<int>();
                int src = j["textures"][ti].value("source", -1);
                if (src >= 0)
                {
                    std::string uri = j["images"][src].value("uri", std::string());
                    if (!uri.empty() && uri.rfind("data:", 0) != 0) outMat->texture = dir + uri; // external image
                }
            }
        }
        if (outHas) *outHas = true;
    }

    std::cout << "glTF loaded: " << path << (isGlb ? " [glb]" : "") << " ("
              << (verts.size() / 8) << " verts, " << (idx.size() / 3) << " tris)" << std::endl;
    return uploadMesh(verts.data(), verts.size() * sizeof(float),
                      idx.data(), idx.size() * sizeof(unsigned int), (GLsizei)idx.size());
}

} // namespace smallgine
