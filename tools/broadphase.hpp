#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace smallgine {

struct AABB
{
    glm::vec3 min{0.0f}, max{0.0f};
    static AABB fromCenter(const glm::vec3& c, const glm::vec3& half) { return { c - half, c + half }; }
    bool overlaps(const AABB& o) const
    {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
};

// Uniform spatial-hash broadphase: bin AABBs into a grid so collision queries
// only test nearby candidates instead of every pair (O(n) vs O(n^2)).
class SpatialHash {
private:
    float cell = 1.0f;
    std::unordered_multimap<long long, int> grid;

    long long key(int x, int y, int z) const
    {
        return ((long long)(x & 0x1FFFFF)) | ((long long)(y & 0x1FFFFF) << 21) | ((long long)(z & 0x1FFFFF) << 42);
    }
    int coord(float v) const { return (int)std::floor(v / cell); }

public:
    void reset(float cellSize) { cell = cellSize > 1e-3f ? cellSize : 1.0f; grid.clear(); }

    void insert(int id, const AABB& b)
    {
        for (int z = coord(b.min.z); z <= coord(b.max.z); ++z)
            for (int y = coord(b.min.y); y <= coord(b.max.y); ++y)
                for (int x = coord(b.min.x); x <= coord(b.max.x); ++x)
                    grid.emplace(key(x, y, z), id);
    }

    // Collect distinct candidate ids sharing any cell with the box (excludes self).
    void query(const AABB& b, int self, std::vector<int>& out) const
    {
        out.clear();
        for (int z = coord(b.min.z); z <= coord(b.max.z); ++z)
            for (int y = coord(b.min.y); y <= coord(b.max.y); ++y)
                for (int x = coord(b.min.x); x <= coord(b.max.x); ++x)
                {
                    auto range = grid.equal_range(key(x, y, z));
                    for (auto it = range.first; it != range.second; ++it)
                        if (it->second != self) out.push_back(it->second);
                }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }
};

} // namespace smallgine
