#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>

namespace smallgine {

// Grid nav mesh over the terrain: cells are walkable where the slope is gentle.
// A* (8-connected) returns world-space waypoints along the terrain surface.
class NavMesh {
private:
    int grid = 0;
    glm::vec3 pos{0.0f}, scale{1.0f};
    std::vector<float> heights;      // unit-space (0..1), grid*grid
    std::vector<unsigned char> walk; // 1 = walkable

    int idx(int x, int z) const { return z * grid + x; }
    float H(int x, int z) const { return heights[idx(x, z)]; }

public:
    void build(const std::vector<float>& h, int g, const glm::vec3& p, const glm::vec3& s, float minUpNormal = 0.86f)
    {
        heights = h; grid = g; pos = p; scale = s;
        walk.assign(g * g, 0);
        float stepX = scale.x / (grid - 1);
        for (int z = 0; z < g; ++z)
            for (int x = 0; x < g; ++x)
            {
                int xl = std::max(x - 1, 0), xr = std::min(x + 1, g - 1);
                int zd = std::max(z - 1, 0), zu = std::min(z + 1, g - 1);
                float dhx = (H(xl, z) - H(xr, z)) * scale.y;
                float dhz = (H(x, zd) - H(x, zu)) * scale.y;
                glm::vec3 n = glm::normalize(glm::vec3(dhx, 2.0f * stepX, dhz));
                walk[idx(x, z)] = (n.y >= minUpNormal) ? 1 : 0;
            }
    }

    bool cellOf(const glm::vec3& w, int& cx, int& cz) const
    {
        float fx = (w.x - pos.x) / scale.x + 0.5f;
        float fz = (w.z - pos.z) / scale.z + 0.5f;
        if (fx < 0 || fx > 1 || fz < 0 || fz > 1) return false;
        cx = (int)std::floor(fx * (grid - 1) + 0.5f); // round to nearest cell
        cz = (int)std::floor(fz * (grid - 1) + 0.5f);
        cx = std::min(std::max(cx, 0), grid - 1);
        cz = std::min(std::max(cz, 0), grid - 1);
        return true;
    }
    glm::vec3 cellCenter(int cx, int cz) const
    {
        float wx = pos.x + ((float)cx / (grid - 1) - 0.5f) * scale.x;
        float wz = pos.z + ((float)cz / (grid - 1) - 0.5f) * scale.z;
        return glm::vec3(wx, pos.y + H(cx, cz) * scale.y, wz);
    }
    bool walkable(int cx, int cz) const { return cx >= 0 && cx < grid && cz >= 0 && cz < grid && walk[idx(cx, cz)]; }
    int size() const { return grid; }
    int walkableCount() const { int n = 0; for (unsigned char c : walk) n += c; return n; }

    // Snap a cell to the nearest walkable one within a small radius (in place).
    void snapWalkable(int& cx, int& cz) const
    {
        if (walkable(cx, cz)) return;
        for (int r = 1; r < 6; ++r)
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx)
                    if (walkable(cx + dx, cz + dz)) { cx += dx; cz += dz; return; }
    }

    // A* over the grid. Returns world waypoints (start-cell..goal-cell), or empty.
    std::vector<glm::vec3> findPath(const glm::vec3& startW, const glm::vec3& goalW) const
    {
        int sx, sz, gx, gz;
        if (!cellOf(startW, sx, sz) || !cellOf(goalW, gx, gz)) return {};
        snapWalkable(sx, sz);
        snapWalkable(gx, gz);
        if (!walkable(sx, sz) || !walkable(gx, gz)) return {};

        auto heur = [&](int x, int z) { return std::hypot((float)(x - gx), (float)(z - gz)); };
        std::vector<float> g(grid * grid, 1e18f);
        std::vector<int> came(grid * grid, -1);
        using PQ = std::pair<float, int>;
        std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
        g[idx(sx, sz)] = 0.0f;
        open.push({heur(sx, sz), idx(sx, sz)});

        while (!open.empty())
        {
            int cur = open.top().second; open.pop();
            int cx = cur % grid, cz = cur / grid;
            if (cx == gx && cz == gz) break;
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (!dx && !dz) continue;
                    int nx = cx + dx, nz = cz + dz;
                    if (!walkable(nx, nz)) continue;
                    if (dx && dz && (!walkable(cx + dx, cz) || !walkable(cx, cz + dz))) continue; // no corner cut
                    float step = (dx && dz) ? 1.41421f : 1.0f;
                    float ng = g[cur] + step;
                    if (ng < g[idx(nx, nz)])
                    {
                        g[idx(nx, nz)] = ng;
                        came[idx(nx, nz)] = cur;
                        open.push({ng + heur(nx, nz), idx(nx, nz)});
                    }
                }
        }

        if (came[idx(gx, gz)] < 0 && !(sx == gx && sz == gz)) return {};
        std::vector<glm::vec3> path;
        for (int c = idx(gx, gz); c != -1; c = came[c])
        {
            path.push_back(cellCenter(c % grid, c / grid));
            if (c == idx(sx, sz)) break;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
};

} // namespace smallgine
