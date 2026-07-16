#pragma once
#include "../platform/glcontext.hpp"
#include <string>

namespace smallgine {

struct Texture
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
};

// Load an image asset (by relative key, via the VFS/pack) into a GL texture
// (forced RGBA). id == 0 on failure.
Texture loadTexture(const std::string& rel);

// Procedural checkerboard, no asset file needed. Requires a current GL context.
Texture makeCheckerTexture(int size = 8);

// Procedural rock albedo: multi-octave noise, mipmapped + trilinear. Its average
// is a neutral rock grey (not a saturated color) so it reads well when minified.
Texture makeRockTexture(int size = 256);

// Landscape palette ramp for splat-painting terrain: sampled at UV = (elevation,
// steepness). The horizontal axis walks sand -> grass -> forest -> rock -> snow by
// height; the vertical axis fades toward bare rock on steep slopes (cliffs). Pair
// with makeHeightMesh(..., paletteUV = true) to color terrain by shape.
Texture makeTerrainPalette(int size = 128);

// Re-upload an image into an existing GL texture id (hot-reload; keeps the id
// so cached node references stay valid). No-op on load failure.
void reloadTexture(GLuint id, const std::string& rel);

} // namespace smallgine
