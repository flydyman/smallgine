#pragma once
#include "../platform/vfs.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <iostream>

namespace smallgine {

// Recursively pack every regular file under `srcDir` into one SGPK file. Stored
// keys are `prefix + "/" + <path relative to srcDir>` with forward slashes, so
// packing the "assets" folder with prefix "assets" yields keys like
// "assets/test.tga" — exactly what the engine's loaders request. The reader
// lives in platform/vfs.hpp. Returns the file count, or -1 on error.
inline int packDirectory(const std::string& srcDir, const std::string& outFile,
                         const std::string& prefix)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(srcDir, ec)) { std::cerr << "Not a directory: " << srcDir << "\n"; return -1; }

    struct Entry { std::string key; std::vector<unsigned char> data; };
    std::vector<Entry> entries;
    for (fs::recursive_directory_iterator it(srcDir, ec), end; it != end; it.increment(ec))
    {
        if (ec) { std::cerr << "Walk error: " << ec.message() << "\n"; return -1; }
        if (!it->is_regular_file(ec)) continue;
        std::string rel = fs::relative(it->path(), srcDir, ec).generic_string();
        std::string key = prefix.empty() ? rel : prefix + "/" + rel;
        std::ifstream f(it->path(), std::ios::binary);
        if (!f) { std::cerr << "Skip (unreadable): " << it->path() << "\n"; continue; }
        entries.push_back({ Pack::normalize(key),
            std::vector<unsigned char>((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>()) });
    }

    // Offsets: header + full index table come first, then the data blob.
    uint64_t off = 4 + 4 + 4;                                     // magic + version + count
    for (auto& e : entries) off += 4 + e.key.size() + 8 + 8;      // per-entry index record
    std::vector<uint64_t> offsets(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) { offsets[i] = off; off += entries[i].data.size(); }

    std::ofstream o(outFile, std::ios::binary);
    if (!o) { std::cerr << "Cannot write: " << outFile << "\n"; return -1; }
    auto w32 = [&](uint32_t v) { o.write((const char*)&v, 4); };
    auto w64 = [&](uint64_t v) { o.write((const char*)&v, 8); };
    o.write("SGPK", 4);
    w32(1);                                                       // version
    w32((uint32_t)entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        w32((uint32_t)entries[i].key.size());
        o.write(entries[i].key.data(), entries[i].key.size());
        w64(offsets[i]);
        w64(entries[i].data.size());
    }
    for (auto& e : entries) o.write((const char*)e.data.data(), e.data.size());
    return (int)entries.size();
}

} // namespace smallgine
