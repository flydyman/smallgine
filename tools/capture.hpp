#pragma once
#include <string>
#include <fstream>
#include <vector>

namespace smallgine {

// Write an uncompressed 24-bit TGA. `rgba` is bottom-up (glReadPixels order),
// which matches TGA's default bottom-left origin, so no row flip is needed.
inline bool writeTGA(const std::string& path, int w, int h, const unsigned char* rgba)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    unsigned char hdr[18] = {0};
    hdr[2] = 2;                        // uncompressed true-color
    hdr[12] = w & 0xFF; hdr[13] = (w >> 8) & 0xFF;
    hdr[14] = h & 0xFF; hdr[15] = (h >> 8) & 0xFF;
    hdr[16] = 24;                      // bits per pixel
    f.write((const char*)hdr, 18);
    std::vector<unsigned char> row(w * 3);
    for (int y = 0; y < h; ++y)
    {
        const unsigned char* src = rgba + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x)
        {
            row[x*3+0] = src[x*4+2]; // B
            row[x*3+1] = src[x*4+1]; // G
            row[x*3+2] = src[x*4+0]; // R
        }
        f.write((const char*)row.data(), row.size());
    }
    return true;
}

} // namespace smallgine
