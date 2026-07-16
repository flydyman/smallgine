#pragma once
#include "paths.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <iostream>

namespace smallgine {

// A "SGPK" package bundles many asset files into one blob so a shipped build can
// load everything from a single file instead of a loose folder tree. On-disk:
//   "SGPK" | u32 version | u32 count
//   count index entries: u32 pathLen, path bytes, u64 offset, u64 size
//   raw data blob (each entry's bytes live at its file-absolute offset)
// Keys use forward slashes and match the engine's asset keys (e.g.
// "assets/test.tga"), so a mounted pack transparently serves the same relative
// paths the loaders already ask for. See tools/packager.hpp for the writer.
class Pack {
public:
    bool load(const std::string& file)
    {
        index.clear();
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;
        blob.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (blob.size() < 12 || std::memcmp(blob.data(), "SGPK", 4) != 0) { blob.clear(); return false; }

        size_t p = 4;
        auto u32 = [&](size_t& q) { uint32_t v; std::memcpy(&v, blob.data() + q, 4); q += 4; return v; };
        auto u64 = [&](size_t& q) { uint64_t v; std::memcpy(&v, blob.data() + q, 8); q += 8; return v; };
        (void)u32(p);                 // version
        uint32_t count = u32(p);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (p + 4 > blob.size()) { blob.clear(); index.clear(); return false; }
            uint32_t len = u32(p);
            if (p + len + 16 > blob.size()) { blob.clear(); index.clear(); return false; }
            std::string name((const char*)blob.data() + p, len); p += len;
            uint64_t off = u64(p), sz = u64(p);
            index[normalize(name)] = { (size_t)off, (size_t)sz };
        }
        return true;
    }

    bool find(const std::string& rel, const unsigned char** ptr, size_t* size) const
    {
        auto it = index.find(normalize(rel));
        if (it == index.end()) return false;
        if (it->second.first + it->second.second > blob.size()) return false;
        *ptr = blob.data() + it->second.first;
        *size = it->second.second;
        return true;
    }

    bool   valid() const { return !index.empty(); }
    size_t count() const { return index.size(); }

    // Canonical key form: forward slashes, no leading "./".
    static std::string normalize(std::string s)
    {
        for (char& c : s) if (c == '\\') c = '/';
        if (s.rfind("./", 0) == 0) s.erase(0, 2);
        return s;
    }

private:
    std::vector<unsigned char> blob;
    std::unordered_map<std::string, std::pair<size_t, size_t>> index;
};

// Process-wide mounted pack (at most one). Empty until a mountPack() succeeds.
inline Pack& g_pack() { static Pack p; return p; }

inline bool mountPack(const std::string& file)
{
    if (!g_pack().load(file)) return false;
    std::cout << "Mounted pack: " << file << " (" << g_pack().count() << " assets)" << std::endl;
    return true;
}

inline bool packMounted() { return g_pack().valid(); }

// Read an asset as bytes: the mounted pack first, else the loose file at
// resolvePath(rel). Returns false only if found in neither.
inline bool readAsset(const std::string& rel, std::vector<unsigned char>& out)
{
    const unsigned char* p; size_t n;
    if (g_pack().valid() && g_pack().find(rel, &p, &n)) { out.assign(p, p + n); return true; }
    std::ifstream f(resolvePath(rel), std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

// Read an asset as text ("" on failure).
inline std::string readAssetText(const std::string& rel)
{
    std::vector<unsigned char> b;
    if (!readAsset(rel, b)) return "";
    return std::string(b.begin(), b.end());
}

inline bool assetExists(const std::string& rel)
{
    const unsigned char* p; size_t n;
    if (g_pack().valid() && g_pack().find(rel, &p, &n)) return true;
    std::ifstream f(resolvePath(rel));
    return (bool)f;
}

} // namespace smallgine
