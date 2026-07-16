#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include "../third_party/stb_truetype.h" // declarations; impl in stb_impl.cpp
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

namespace smallgine {

namespace {

}

// Bitmap-font text renderer (stb_truetype baked atlas, ASCII 32..126).
class TextRenderer {
private:
    GLuint prog = 0, atlas = 0, vao = 0, vbo = 0;
    GLint uScreen = -1, uFont = -1, uColor = -1;
    stbtt_bakedchar cdata[96];
    int atlasW = 512, atlasH = 512;
    float pixelHeight = 24.0f;
    bool ready = false;

public:
    bool init(const std::string& fontPath, float px = 24.0f)
    {
        pixelHeight = px;

        std::vector<unsigned char> ttf;
        if (!readAsset(fontPath, ttf))
        {
            std::cout << "Font load failed: " << fontPath << std::endl;
            return false;
        }

        std::vector<unsigned char> bitmap(atlasW * atlasH);
        int r = stbtt_BakeFontBitmap(ttf.data(), 0, pixelHeight,
                                     bitmap.data(), atlasW, atlasH, 32, 96, cdata);
        if (r == 0)
        {
            std::cout << "Font bake failed: " << fontPath << std::endl;
            return false;
        }

        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        prog = tools::linkProgramFiles("assets/shaders/text.vert", "assets/shaders/text.frag");
        uScreen = glGetUniformLocation(prog, "uScreen");
        uFont = glGetUniformLocation(prog, "uFont");
        uColor = glGetUniformLocation(prog, "uColor");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        ready = true;
        return true;
    }

    // Draw a string at pixel (x, y) = baseline start. Caller sets blend state.
    void draw(const std::string& text, float x, float y, int screenW, int screenH,
              const glm::vec3& color)
    {
        if (!ready) return;

        std::vector<float> verts;
        verts.reserve(text.size() * 24);
        float px = x, py = y;
        for (char ch : text)
        {
            if (ch < 32 || ch > 126) continue;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, atlasW, atlasH, ch - 32, &px, &py, &q, 1);
            // two triangles (x0,y0)-(x1,y1)
            float v[24] = {
                q.x0, q.y0, q.s0, q.t0,  q.x1, q.y0, q.s1, q.t0,  q.x1, q.y1, q.s1, q.t1,
                q.x1, q.y1, q.s1, q.t1,  q.x0, q.y1, q.s0, q.t1,  q.x0, q.y0, q.s0, q.t0,
            };
            verts.insert(verts.end(), v, v + 24);
        }
        if (verts.empty()) return;

        glUseProgram(prog);
        glUniform2f(uScreen, (float)screenW, (float)screenH);
        glUniform3fv(uColor, 1, &color[0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glUniform1i(uFont, 0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
        glBindVertexArray(0);
    }

    void free()
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (atlas) glDeleteTextures(1, &atlas);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
