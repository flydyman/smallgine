#include "texture.hpp"
#include "../third_party/stb_image.h" // declarations only; implementation in stb_impl.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace smallgine {

Texture loadTexture(const char* path)
{
    Texture t;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &t.width, &t.height, &t.channels, 4);
    if (!data)
    {
        std::cout << "Texture load failed: " << path << std::endl;
        return t;
    }

    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width, t.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return t;
}

void reloadTexture(GLuint id, const char* path)
{
    if (!id) return;
    stbi_set_flip_vertically_on_load(1);
    int w = 0, h = 0, c = 0;
    unsigned char* data = stbi_load(path, &w, &h, &c, 4);
    if (!data) { std::cout << "Texture reload failed: " << path << std::endl; return; }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    std::cout << "Texture reloaded: " << path << std::endl;
}

Texture makeCheckerTexture(int size)
{
    std::vector<unsigned char> px(size * size * 4);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            unsigned char v = ((x ^ y) & 1) ? 235 : 30;
            int i = (y * size + x) * 4;
            px[i + 0] = v;
            px[i + 1] = v;
            px[i + 2] = v;
            px[i + 3] = 255;
        }
    }

    Texture t;
    t.width = t.height = size;
    t.channels = 4;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return t;
}

Texture makeRockTexture(int size)
{
    // Hash-based value noise, summed over octaves, tinted between two rock greys.
    auto hash = [](int x, int y) {
        unsigned h = (unsigned)(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
    };
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    // Periodic (per) so the whole texture tiles seamlessly when UV-repeated.
    auto vnoise = [&](float u, float v, int per) {
        int xi = (int)std::floor(u), yi = (int)std::floor(v);
        float fx = u - xi, fy = v - yi;
        float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
        auto w = [&](int a) { return ((a % per) + per) % per; };
        float a = hash(w(xi),     w(yi)),     b = hash(w(xi + 1), w(yi));
        float c = hash(w(xi),     w(yi + 1)), d = hash(w(xi + 1), w(yi + 1));
        return lerp(lerp(a, b, sx), lerp(c, d, sx), sy);
    };

    std::vector<unsigned char> px((size_t)size * size * 4);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            float u = (float)x / size, v = (float)y / size;
            // fBm: low octaves give big blotches that survive minification, high
            // octaves give crisp speckle up close.
            float n = 0.0f, amp = 0.5f; int freq = 4;
            for (int o = 0; o < 5; ++o) { n += amp * vnoise(u * freq, v * freq, freq); amp *= 0.5f; freq *= 2; }
            n = std::min(1.0f, std::max(0.0f, n));
            // Two desaturated rock tones (slightly warm); wide range for visible relief.
            unsigned char r = (unsigned char)(lerp(38, 178, n));
            unsigned char g = (unsigned char)(lerp(34, 166, n));
            unsigned char b = (unsigned char)(lerp(30, 150, n));
            int i = (y * size + x) * 4;
            px[i + 0] = r; px[i + 1] = g; px[i + 2] = b; px[i + 3] = 255;
        }

    Texture t;
    t.width = t.height = size;
    t.channels = 4;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return t;
}

Texture makeTerrainPalette(int size)
{
    struct Stop { float h; float r, g, b; };
    // Elevation ramp for flat ground (0 = lowest, 1 = highest).
    static const Stop stops[] = {
        { 0.00f, 0.80f, 0.74f, 0.52f }, // shoreline sand
        { 0.14f, 0.76f, 0.70f, 0.48f }, // sand
        { 0.24f, 0.42f, 0.56f, 0.26f }, // low grass
        { 0.45f, 0.28f, 0.50f, 0.22f }, // meadow
        { 0.62f, 0.20f, 0.40f, 0.18f }, // forest
        { 0.74f, 0.46f, 0.42f, 0.36f }, // exposed rock
        { 0.86f, 0.60f, 0.58f, 0.55f }, // scree
        { 0.94f, 0.92f, 0.93f, 0.97f }, // snow
        { 1.00f, 0.98f, 0.99f, 1.00f }, // peak snow
    };
    const int N = (int)(sizeof(stops) / sizeof(stops[0]));
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto ramp = [&](float h, float& r, float& g, float& b) {
        h = std::min(1.0f, std::max(0.0f, h));
        for (int i = 1; i < N; ++i)
            if (h <= stops[i].h)
            {
                const Stop& a = stops[i - 1]; const Stop& c = stops[i];
                float t = (h - a.h) / std::max(1e-5f, c.h - a.h);
                r = lerp(a.r, c.r, t); g = lerp(a.g, c.g, t); b = lerp(a.b, c.b, t);
                return;
            }
        r = stops[N - 1].r; g = stops[N - 1].g; b = stops[N - 1].b;
    };

    std::vector<unsigned char> px((size_t)size * size * 4);
    for (int y = 0; y < size; ++y)
    {
        float steep = (float)y / (size - 1);                 // 0 flat .. 1 vertical
        // Cliffs turn to bare rock, but the highest peaks keep their snow cap.
        for (int x = 0; x < size; ++x)
        {
            float h = (float)x / (size - 1);
            float r, g, b; ramp(h, r, g, b);
            float rockMix = (h > 0.88f) ? 0.0f : (steep - 0.32f) / 0.35f;
            rockMix = std::min(1.0f, std::max(0.0f, rockMix));
            r = lerp(r, 0.38f, rockMix); g = lerp(g, 0.34f, rockMix); b = lerp(b, 0.30f, rockMix);
            int i = (y * size + x) * 4;
            px[i + 0] = (unsigned char)(std::min(1.0f, r) * 255.0f);
            px[i + 1] = (unsigned char)(std::min(1.0f, g) * 255.0f);
            px[i + 2] = (unsigned char)(std::min(1.0f, b) * 255.0f);
            px[i + 3] = 255;
        }
    }

    Texture t;
    t.width = t.height = size;
    t.channels = 4;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

} // namespace smallgine
