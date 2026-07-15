#pragma once
#include "../platform/glcontext.hpp"
#include "../third_party/stb_image.h" // declarations only; implementation lives in stb_impl.cpp
#include <iostream>
#include <vector>

namespace smallgine {

struct Texture
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
};

// Load an image file into a GL texture (forced RGBA). id == 0 on failure.
inline Texture loadTexture(const char* path)
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

// Procedural checkerboard, no asset file needed. Requires a current GL context.
inline Texture makeCheckerTexture(int size = 8)
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

} // namespace smallgine
