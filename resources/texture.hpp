#pragma once
#include "../platform/glcontext.hpp"

namespace smallgine {

struct Texture
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
};

// Load an image file into a GL texture (forced RGBA). id == 0 on failure.
Texture loadTexture(const char* path);

// Procedural checkerboard, no asset file needed. Requires a current GL context.
Texture makeCheckerTexture(int size = 8);

// Procedural rock albedo: multi-octave noise, mipmapped + trilinear. Its average
// is a neutral rock grey (not a saturated color) so it reads well when minified.
Texture makeRockTexture(int size = 256);

// Re-upload an image into an existing GL texture id (hot-reload; keeps the id
// so cached node references stay valid). No-op on load failure.
void reloadTexture(GLuint id, const char* path);

} // namespace smallgine
