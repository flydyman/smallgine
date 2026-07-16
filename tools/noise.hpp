#pragma once
#include <cstdint>
#include <cmath>
#include <functional>

// A small, self-contained library of the popular procedural-noise generators.
// Everything is header-only, deterministic (driven by an explicit integer seed),
// and free of engine/GL dependencies so it can be unit-tested without a window.
//
// Base generators (value, Perlin gradient, simplex, Worley/cellular) return the
// canonical ~[-1,1] range so they compose cleanly; the fractal wrappers (fBm,
// ridged, turbulence, domain warp) build detail by summing octaves. sample01()
// is a convenience dispatcher that normalizes any generator to [0,1] for use as
// a heightmap or texture value.
namespace smallgine {
namespace noise {

    // ---- Integer hashing (fast, well-mixed avalanche) --------------------------

    inline uint32_t hashU(uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16; return x;
    }

    inline uint32_t hash2(int x, int y, uint32_t seed)
    {
        uint32_t h = seed * 0x9e3779b9U;
        h = hashU(h ^ (uint32_t)(x * 0x8da6b343));
        h = hashU(h ^ (uint32_t)(y * 0xd8163841));
        return h;
    }

    inline uint32_t hash3(int x, int y, int z, uint32_t seed)
    {
        uint32_t h = seed * 0x9e3779b9U;
        h = hashU(h ^ (uint32_t)(x * 0x8da6b343));
        h = hashU(h ^ (uint32_t)(y * 0xd8163841));
        h = hashU(h ^ (uint32_t)(z * 0xcb1ab31f));
        return h;
    }

    // Uniform value in [0,1) from a hash.
    inline float toUnit(uint32_t h) { return (float)h * (1.0f / 4294967296.0f); }

    // White noise: independent per-cell random value, no interpolation.
    inline float white2(int x, int y, uint32_t seed = 0) { return toUnit(hash2(x, y, seed)) * 2.0f - 1.0f; }

    // ---- Shared helpers --------------------------------------------------------

    inline int   ifloor(float v) { return (int)std::floor(v); }
    inline float fade(float t)   { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); } // quintic
    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // 8 evenly spaced unit gradient directions (for value/Perlin/simplex).
    inline void grad2(uint32_t h, float& gx, float& gy)
    {
        static const float G[8][2] = {
            { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
            { 0.70710678f, 0.70710678f }, { -0.70710678f, 0.70710678f },
            { 0.70710678f, -0.70710678f }, { -0.70710678f, -0.70710678f },
        };
        const float* g = G[h & 7]; gx = g[0]; gy = g[1];
    }

    // ---- Value noise (bilerp of per-lattice random values, quintic fade) -------

    inline float value2(float x, float y, uint32_t seed = 0)
    {
        int x0 = ifloor(x), y0 = ifloor(y);
        float fx = x - x0, fy = y - y0;
        float u = fade(fx), v = fade(fy);
        float v00 = toUnit(hash2(x0,   y0,   seed));
        float v10 = toUnit(hash2(x0+1, y0,   seed));
        float v01 = toUnit(hash2(x0,   y0+1, seed));
        float v11 = toUnit(hash2(x0+1, y0+1, seed));
        float n = lerp(lerp(v00, v10, u), lerp(v01, v11, u), v);
        return n * 2.0f - 1.0f; // [0,1) -> [-1,1]
    }

    // ---- Perlin gradient noise (Ken Perlin's improved noise) -------------------

    inline float perlin2(float x, float y, uint32_t seed = 0)
    {
        int x0 = ifloor(x), y0 = ifloor(y);
        float fx = x - x0, fy = y - y0;
        float u = fade(fx), v = fade(fy);
        auto dot = [&](int ix, int iy, float dx, float dy) {
            float gx, gy; grad2(hash2(ix, iy, seed), gx, gy);
            return gx * dx + gy * dy;
        };
        float n00 = dot(x0,   y0,   fx,        fy);
        float n10 = dot(x0+1, y0,   fx - 1.0f, fy);
        float n01 = dot(x0,   y0+1, fx,        fy - 1.0f);
        float n11 = dot(x0+1, y0+1, fx - 1.0f, fy - 1.0f);
        float n = lerp(lerp(n00, n10, u), lerp(n01, n11, u), v);
        return n * 1.41421356f; // roughly fill [-1,1]
    }

    inline float perlin3(float x, float y, float z, uint32_t seed = 0)
    {
        // 12 edge-of-cube gradient directions (Perlin's canonical set).
        static const float G3[12][3] = {
            { 1,1,0 },{ -1,1,0 },{ 1,-1,0 },{ -1,-1,0 },
            { 1,0,1 },{ -1,0,1 },{ 1,0,-1 },{ -1,0,-1 },
            { 0,1,1 },{ 0,-1,1 },{ 0,1,-1 },{ 0,-1,-1 },
        };
        int x0 = ifloor(x), y0 = ifloor(y), z0 = ifloor(z);
        float fx = x - x0, fy = y - y0, fz = z - z0;
        float u = fade(fx), v = fade(fy), w = fade(fz);
        auto dot = [&](int ix, int iy, int iz, float dx, float dy, float dz) {
            const float* g = G3[hash3(ix, iy, iz, seed) % 12];
            return g[0] * dx + g[1] * dy + g[2] * dz;
        };
        float n000 = dot(x0,   y0,   z0,   fx,      fy,      fz);
        float n100 = dot(x0+1, y0,   z0,   fx-1.0f, fy,      fz);
        float n010 = dot(x0,   y0+1, z0,   fx,      fy-1.0f, fz);
        float n110 = dot(x0+1, y0+1, z0,   fx-1.0f, fy-1.0f, fz);
        float n001 = dot(x0,   y0,   z0+1, fx,      fy,      fz-1.0f);
        float n101 = dot(x0+1, y0,   z0+1, fx-1.0f, fy,      fz-1.0f);
        float n011 = dot(x0,   y0+1, z0+1, fx,      fy-1.0f, fz-1.0f);
        float n111 = dot(x0+1, y0+1, z0+1, fx-1.0f, fy-1.0f, fz-1.0f);
        float nx00 = lerp(n000, n100, u), nx10 = lerp(n010, n110, u);
        float nx01 = lerp(n001, n101, u), nx11 = lerp(n011, n111, u);
        return lerp(lerp(nx00, nx10, v), lerp(nx01, nx11, v), w) * 1.15f;
    }

    // ---- Simplex noise (Gustavson 2D, unit-gradient variant) -------------------

    inline float simplex2(float x, float y, uint32_t seed = 0)
    {
        const float F2 = 0.36602540378f; // (sqrt(3)-1)/2
        const float G2 = 0.21132486540f; // (3-sqrt(3))/6
        float s = (x + y) * F2;
        int i = ifloor(x + s), j = ifloor(y + s);
        float t = (i + j) * G2;
        float x0 = x - (i - t), y0 = y - (j - t);
        int i1 = (x0 > y0) ? 1 : 0, j1 = (x0 > y0) ? 0 : 1;
        float x1 = x0 - i1 + G2,        y1 = y0 - j1 + G2;
        float x2 = x0 - 1.0f + 2.0f*G2, y2 = y0 - 1.0f + 2.0f*G2;
        auto corner = [&](int ci, int cj, float dx, float dy) {
            float tt = 0.5f - dx*dx - dy*dy;
            if (tt < 0.0f) return 0.0f;
            float gx, gy; grad2(hash2(ci, cj, seed), gx, gy);
            tt *= tt;
            return tt * tt * (gx * dx + gy * dy);
        };
        float n = corner(i, j, x0, y0)
                + corner(i + i1, j + j1, x1, y1)
                + corner(i + 1, j + 1, x2, y2);
        return 70.0f * n; // scale to ~[-1,1]
    }

    // ---- Worley / cellular (Voronoi) noise -------------------------------------

    // Returns F1 (distance to the nearest feature point) in ~[0,1]. Optionally
    // reports F2 too; the classic F2-F1 gives crack/ridge patterns.
    inline float worley2(float x, float y, uint32_t seed = 0, float* outF2 = nullptr)
    {
        int xi = ifloor(x), yi = ifloor(y);
        float f1 = 1e9f, f2 = 1e9f;
        for (int gy = -1; gy <= 1; ++gy)
            for (int gx = -1; gx <= 1; ++gx)
            {
                int cx = xi + gx, cy = yi + gy;
                uint32_t h = hash2(cx, cy, seed);
                float px = cx + toUnit(h);
                float py = cy + toUnit(hashU(h));
                float dx = px - x, dy = py - y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < f1) { f2 = f1; f1 = d; }
                else if (d < f2) { f2 = d; }
            }
        if (outF2) *outF2 = f2 > 1.0f ? 1.0f : f2;
        return f1 > 1.0f ? 1.0f : f1;
    }

    // ---- Fractal combiners (parameterized over any base generator) -------------

    struct Fractal
    {
        int   octaves    = 5;
        float lacunarity = 2.0f;  // frequency multiplier per octave
        float gain       = 0.5f;  // amplitude multiplier per octave
        float frequency  = 1.0f;  // base frequency
    };

    // Fractal Brownian motion: weighted sum of octaves. Result in ~[-1,1].
    template <class Base>
    inline float fbm(Base&& base, float x, float y, const Fractal& p)
    {
        float sum = 0.0f, amp = 0.5f, freq = p.frequency, norm = 0.0f;
        for (int o = 0; o < p.octaves; ++o)
        {
            sum  += amp * base(x * freq, y * freq);
            norm += amp;
            amp  *= p.gain;
            freq *= p.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    // Ridged multifractal: sharp ridge lines from folded, squared octaves. [0,1].
    template <class Base>
    inline float ridged(Base&& base, float x, float y, const Fractal& p)
    {
        float sum = 0.0f, amp = 0.5f, freq = p.frequency, norm = 0.0f;
        for (int o = 0; o < p.octaves; ++o)
        {
            float n = 1.0f - std::fabs(base(x * freq, y * freq));
            n *= n;
            sum  += amp * n;
            norm += amp;
            amp  *= p.gain;
            freq *= p.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    // Turbulence (billow): sum of absolute octaves — puffy, cloud-like. [0,1].
    template <class Base>
    inline float turbulence(Base&& base, float x, float y, const Fractal& p)
    {
        float sum = 0.0f, amp = 0.5f, freq = p.frequency, norm = 0.0f;
        for (int o = 0; o < p.octaves; ++o)
        {
            sum  += amp * std::fabs(base(x * freq, y * freq));
            norm += amp;
            amp  *= p.gain;
            freq *= p.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    // Domain warp: perturb the sample position by noise before sampling again.
    // Produces swirling, organic distortion. Result in ~[-1,1].
    inline float warp2(float x, float y, uint32_t seed = 0, float amount = 4.0f)
    {
        float qx = perlin2(x + 0.0f, y + 0.0f, seed);
        float qy = perlin2(x + 5.2f, y + 1.3f, seed);
        return perlin2(x + amount * qx, y + amount * qy, seed);
    }

    // ---- Unified dispatcher (for demos / previews) -----------------------------

    enum class Type
    {
        White, Value, Perlin, Simplex,
        Worley, Cracks, Fbm, Ridged, Turbulence, Warp,
        Count
    };

    inline const char* name(Type t)
    {
        switch (t)
        {
            case Type::White:      return "White noise";
            case Type::Value:      return "Value noise";
            case Type::Perlin:     return "Perlin (gradient)";
            case Type::Simplex:    return "Simplex";
            case Type::Worley:     return "Worley (cellular F1)";
            case Type::Cracks:     return "Worley cracks (F2-F1)";
            case Type::Fbm:        return "fBm (Perlin, 5 oct)";
            case Type::Ridged:     return "Ridged multifractal";
            case Type::Turbulence: return "Turbulence (billow)";
            case Type::Warp:       return "Domain warp";
            default:               return "?";
        }
    }

    // Sample any generator normalized to [0,1], ready for heightmaps/textures.
    // (Gradient/simplex noise can graze just past ±1, so the result is clamped.)
    inline float sample01(Type t, float x, float y, uint32_t seed = 0)
    {
        Fractal fp; // defaults
        auto perlinBase = [seed](float px, float py) { return perlin2(px, py, seed); };
        float v;
        switch (t)
        {
            case Type::White:   v = white2(ifloor(x), ifloor(y), seed) * 0.5f + 0.5f; break;
            case Type::Value:   v = value2(x, y, seed) * 0.5f + 0.5f; break;
            case Type::Perlin:  v = perlin2(x, y, seed) * 0.5f + 0.5f; break;
            case Type::Simplex: v = simplex2(x, y, seed) * 0.5f + 0.5f; break;
            case Type::Worley:  v = worley2(x, y, seed); break;
            case Type::Cracks:  { float f2; float f1 = worley2(x, y, seed, &f2); v = f2 - f1; } break;
            case Type::Fbm:     v = fbm(perlinBase, x, y, fp) * 0.5f + 0.5f; break;
            case Type::Ridged:  v = ridged(perlinBase, x, y, fp); break;
            case Type::Turbulence: v = turbulence(perlinBase, x, y, fp); break;
            case Type::Warp:    v = warp2(x, y, seed) * 0.5f + 0.5f; break;
            default:            v = 0.0f; break;
        }
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

} // namespace noise
} // namespace smallgine
