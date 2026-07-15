#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

namespace smallgine {

namespace {
    const char* kSkyVert =
        "#version 310 es\n"
        "layout(location = 0) in vec3 aPos;\n"
        "uniform mat4 uView;\n"
        "uniform mat4 uProj;\n"
        "out vec3 vDir;\n"
        "void main() {\n"
        "    vDir = aPos;\n"
        "    vec4 p = uProj * uView * vec4(aPos, 1.0);\n"
        "    gl_Position = p.xyww;\n" // force depth = 1.0 (far plane)
        "}\n";

    const char* kSkyFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec3 vDir;\n"
        "uniform samplerCube uSky;\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = texture(uSky, normalize(vDir)); }\n";

    inline glm::vec3 skyFaceDir(int face, float u, float v)
    {
        switch (face)
        {
            case 0: return glm::vec3( 1.0f,  -v,  -u); // +X
            case 1: return glm::vec3(-1.0f,  -v,   u); // -X
            case 2: return glm::vec3(  u,  1.0f,   v); // +Y
            case 3: return glm::vec3(  u, -1.0f,  -v); // -Y
            case 4: return glm::vec3(  u,   -v, 1.0f); // +Z
            default:return glm::vec3( -u,   -v,-1.0f); // -Z
        }
    }

    inline glm::vec3 skyColor(glm::vec3 d)
    {
        d = glm::normalize(d);
        float t = glm::clamp(d.y * 0.5f + 0.5f, 0.0f, 1.0f);
        glm::vec3 horizon(0.85f, 0.55f, 0.35f);
        glm::vec3 zenith(0.15f, 0.30f, 0.60f);
        glm::vec3 c = glm::mix(horizon, zenith, t);
        glm::vec3 sun = glm::normalize(glm::vec3(0.4f, 0.7f, 0.6f));
        float s = std::pow(std::max(glm::dot(d, sun), 0.0f), 64.0f);
        c += glm::vec3(1.0f, 0.9f, 0.7f) * s;
        return glm::clamp(c, 0.0f, 1.0f);
    }
}

class Skybox {
private:
    GLuint vao = 0, vbo = 0, tex = 0, prog = 0;
    GLint uView = -1, uProj = -1, uSky = -1;

public:
    void init(int size = 64)
    {
        prog = tools::linkProgram(kSkyVert, kSkyFrag);
        uView = glGetUniformLocation(prog, "uView");
        uProj = glGetUniformLocation(prog, "uProj");
        uSky = glGetUniformLocation(prog, "uSky");

        // Procedural gradient cubemap (no asset files).
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        std::vector<unsigned char> px(size * size * 3);
        for (int f = 0; f < 6; ++f)
        {
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    float u = 2.0f * (x + 0.5f) / size - 1.0f;
                    float v = 2.0f * (y + 0.5f) / size - 1.0f;
                    glm::vec3 c = skyColor(skyFaceDir(f, u, v));
                    int i = (y * size + x) * 3;
                    px[i + 0] = (unsigned char)(c.r * 255.0f);
                    px[i + 1] = (unsigned char)(c.g * 255.0f);
                    px[i + 2] = (unsigned char)(c.b * 255.0f);
                }
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB, size, size, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, px.data());
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Unit cube, positions only.
        static const float verts[] = {
            -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
            -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
            -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
             1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
            -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
        };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Draw the sky behind everything already rendered.
    void draw(const glm::mat4& view, const glm::mat4& proj)
    {
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE); // viewed from inside the cube

        glUseProgram(prog);
        glm::mat4 v = glm::mat4(glm::mat3(view)); // strip translation
        glUniformMatrix4fv(uView, 1, GL_FALSE, &v[0][0]);
        glUniformMatrix4fv(uProj, 1, GL_FALSE, &proj[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        glUniform1i(uSky, 0);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);
    }

    void free()
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (tex) glDeleteTextures(1, &tex);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
